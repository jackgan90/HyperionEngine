#include "NativeMigration.h"
#include "Hyperion/Assets/AssetAuthoring.h"
#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <set>

namespace Hyperion
{
namespace
{
bool IsWithin(const std::filesystem::path& InPath, const std::filesystem::path& InRoot)
{
	const auto Relative = InPath.lexically_relative(InRoot);
	return !Relative.empty() && !Relative.is_absolute() && *Relative.begin() != "..";
}

bool IsInternal(const std::filesystem::path& InPath, const std::filesystem::path& InRoot)
{
	return *InPath.lexically_relative(InRoot).begin() == ".assets";
}

std::vector<std::filesystem::path> PublicRoots(IFileSystem& InFiles, const std::filesystem::path& InRoot)
{
	std::vector<std::filesystem::path> Result;
	std::vector<std::filesystem::path> Pending{InRoot};
	while (!Pending.empty())
	{
		const auto Directory = Pending.back();
		Pending.pop_back();
		for (const auto& Entry : InFiles.ListDirectory(Directory))
		{
			const auto Name = PathToUtf8(Entry.Path.filename());
			if (Name == ".assets" || Name == ".cache" || Name == ".git" || Name.starts_with(".publish-"))
			{
				continue;
			}
			if (!Entry.Error.empty())
			{
				throw std::runtime_error(Entry.Error);
			}
			if (Entry.bDirectory)
			{
				Pending.push_back(Entry.Path);
			}
			else if (Entry.Path.extension() == ".hasset" && Name != "Catalog.hasset" && Name != ".asset-library.hasset")
			{
				Result.push_back(Entry.Path);
			}
		}
	}
	std::sort(Result.begin(), Result.end());
	return Result;
}

struct FNativeMigration
{
	FIOService& IO;
	FAssetService Assets;
	std::filesystem::path Root;
	std::filesystem::path Output;
	std::map<std::filesystem::path, std::shared_ptr<const FLoadedAsset>> Nodes;
	std::map<std::string, std::string> Aliases;
	std::map<std::string, std::filesystem::path> Targets;
	std::map<std::string, std::map<std::string, std::string>> ProductIds;
	std::map<std::filesystem::path, FEncodedAsset> Staged;

	FNativeMigration(FIOService& InIO, std::filesystem::path InRoot, std::filesystem::path InOutput)
	    : IO(InIO), Assets(InIO), Root(std::move(InRoot)), Output(std::move(InOutput))
	{
		RegisterSceneAssetTypes(Assets.Types());
	}

	std::string Canonical(std::string InId) const
	{
		const auto It = Aliases.find(InId);
		return It == Aliases.end() ? InId : It->second;
	}

	void Gather()
	{
		std::map<std::pair<std::string, std::string>, std::vector<std::string>> PublicPayloads;
		for (const auto& Path : PublicRoots(*IO.FileSystem(), Root))
		{
			const auto Graph = Assets.LoadGraphAsync(Path).Get(IO.TaskSystem());
			if (!Graph->Failures.empty())
			{
				throw std::runtime_error(PathToUtf8(Path) + ": " + Graph->Failures.front().Error);
			}
			Nodes.insert(Graph->Assets.begin(), Graph->Assets.end());
			const auto& Header = Graph->Root->Header;
			PublicPayloads[{Header.TypeId, Header.Revision}].push_back(Header.Id);
		}
		for (const auto& [Path, Asset] : Nodes)
		{
			if (!IsWithin(Path, Root) || !IsInternal(Path, Root))
			{
				continue;
			}
			const auto Found = PublicPayloads.find({Asset->Header.TypeId, Asset->Header.Revision});
			if (Found != PublicPayloads.end() && Found->second.size() == 1)
			{
				Aliases[Asset->Header.Id] = Found->second.front();
			}
		}
		for (const auto& [Path, Asset] : Nodes)
		{
			if (!IsWithin(Path, Root) || Aliases.contains(Asset->Header.Id))
			{
				continue;
			}
			const auto Target = IsInternal(Path, Root)
			                        ? AssetProductPath(*Asset->Type, Asset->Object.get(), Root, Asset->Header.Id)
			                        : Path;
			if (!Targets.emplace(Asset->Header.Id, Target).second)
			{
				throw std::runtime_error("Multiple reachable files claim asset ID " + Asset->Header.Id);
			}
		}
	}

	std::map<std::string, std::string> Mappings(const FAssetProvenance& InImport) const
	{
		std::map<std::string, std::string> Result;
		for (const auto& [Key, Id] : InImport.OutputIds)
		{
			const auto Current = Canonical(Id);
			if (Targets.contains(Current))
			{
				auto NewKey = Key;
				for (const auto& [OldId, NewId] : Aliases)
				{
					const auto Prefix = "native/" + OldId + "|";
					if (NewKey.starts_with(Prefix))
					{
						NewKey.replace(7, OldId.size(), NewId);
					}
				}
				Result.emplace(std::move(NewKey), Current);
			}
		}
		return Result;
	}

	void Rewrite(const FLoadedAsset& InAsset, void* InObject)
	{
		VisitRecord(*InAsset.Type, InObject,
		            [&](const FRecordDescriptor& InType, const void* InValue, std::string_view)
		            {
			            if (InType.CppType != typeid(FAssetRef))
			            {
				            return;
			            }
			            auto& Reference = *const_cast<FAssetRef*>(static_cast<const FAssetRef*>(InValue));
			            const auto Path = Assets.Resolve(Reference, InAsset.Path);
			            const auto& Target = *Nodes.at(Path);
			            const auto Id = Canonical(Target.Header.Id);
			            Reference = {
			                Id, PathToUtf8(Targets.contains(Id) ? Targets.at(Id) : Path), Target.Header.TypeId, {}};
		            });
	}

	void Stage()
	{
		for (const auto& [Path, Asset] : Nodes)
		{
			if (Asset->Header.Import)
			{
				for (const auto& [Key, Id] : Mappings(*Asset->Header.Import))
				{
					if (Key != "$root")
					{
						ProductIds[Id][Key] = Id;
					}
				}
			}
		}
		for (const auto& [Path, Asset] : Nodes)
		{
			const auto& Id = Asset->Header.Id;
			if (!Targets.contains(Id) || Aliases.contains(Id))
			{
				continue;
			}
			auto Object = ReadRecord(*Asset->Type, WriteRecord(*Asset->Type, Asset->Object.get()));
			Rewrite(*Asset, Object.get());
			auto Header = Asset->Header;
			if (Header.Import)
			{
				Header.Import->OutputIds = Mappings(*Header.Import);
				Header.Import->Settings["publication"] = "current-native-v1";
			}
			else if (ProductIds.contains(Id))
			{
				Header.Import.emplace();
				Header.Import->Importer = "hyperion.native-product";
				Header.Import->ImporterVersion = 1;
				Header.Import->OutputIds = ProductIds.at(Id);
			}
			if (Header.Import && Header.TypeId == "hyperion.textureasset")
			{
				auto Record = WriteRecord(*Asset->Type, Object.get());
				std::get<FArchiveNode::FObject>(
				    std::get<FArchiveNode::FObject>(Record.Value).at("fields").Value)["name"] =
				    WriteValue(std::string{});
				Header.Import->Settings["texture_content"] = HashArchive(Record);
			}
			Staged.emplace(Output / Targets.at(Id).lexically_relative(Root),
			               EncodeAsset(*Asset->Type, Object.get(), Header));
		}
	}
};
} // namespace

void MigrateNativeLibrary(FIOService& InIO, const std::filesystem::path& InRoot, const std::filesystem::path& InOutput,
                          std::ostream& InLog)
{
	const auto Root = InIO.FileSystem()->Normalize(InRoot);
	FLocalFileSystem Local;
	const auto Output = Local.Normalize(InOutput);
	if (IsPackagePath(InIO.FileSystem()->Normalize(Output)))
	{
		throw std::invalid_argument("Migration staging must be outside mounted content directories");
	}
	if (!IsPackagePath(Root) || Root.parent_path() != "/")
	{
		throw std::invalid_argument("Migration requires a mounted content root, such as /Game");
	}
	if (Local.Exists(Output) && !Local.ListDirectory(Output).empty())
	{
		throw std::invalid_argument("Migration output must be a new or empty staging directory");
	}
	FNativeMigration Migration(InIO, Root, Output);
	Migration.Gather();
	Migration.Stage();
	for (const auto& [Path, Asset] : Migration.Staged)
	{
		InIO.WriteAsync(Path, Asset.Bytes).Get(InIO.TaskSystem());
	}
	for (const auto& [OldId, NewId] : Migration.Aliases)
	{
		InLog << "alias " << OldId << " -> " << NewId << '\n';
	}
	InLog << "Staged " << Migration.Staged.size() << " current native assets at " << PathToUtf8(Output) << '\n';
}

void ValidateNativeLibrary(FIOService& InIO, const std::filesystem::path& InRoot, std::ostream& InLog)
{
	FAssetService Assets(InIO);
	RegisterSceneAssetTypes(Assets.Types());
	const auto Root = InIO.FileSystem()->Normalize(InRoot);
	if (const auto* Mounted = dynamic_cast<FMountedFileSystem*>(InIO.FileSystem().get()))
	{
		for (const auto& Mount : Mounted->GetMounts())
		{
			const auto Found = DiscoverAssets(*InIO.FileSystem(), Mount.Root);
			if (!Found.Errors.empty())
			{
				throw std::runtime_error(Found.Errors.begin()->second);
			}
			Assets.AddAssetIndex(BuildAssetIndex(Found.Entries), Mount.Root);
		}
	}
	const auto Found = DiscoverAssets(*InIO.FileSystem(), Root);
	if (!Found.Errors.empty())
	{
		throw std::runtime_error(Found.Errors.begin()->second);
	}
	const auto& Entries = Found.Entries;
	if (!IsPackagePath(Root))
	{
		Assets.AddAssetIndex(BuildAssetIndex(Entries), Root);
	}
	for (const auto& Entry : Entries)
	{
		const auto Graph = Assets.LoadGraphAsync(Entry.Path).Get(InIO.TaskSystem());
		if (!Graph->Failures.empty())
		{
			throw std::runtime_error(PathToUtf8(Entry.Path) + ": " + Graph->Failures.front().Error);
		}
		for (const auto& Dependency : Entry.Header.Dependencies)
		{
			if (!Dependency.Reference.Revision.empty())
			{
				throw std::runtime_error("Pinned authoring reference: " + PathToUtf8(Entry.Path));
			}
		}
	}
	InLog << "Validated " << Entries.size() << " current native assets and dependency graphs\n";
}
} // namespace Hyperion
