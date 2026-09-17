#include "AssetCommands.h"
#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/MaterialImport.h"
#include "Hyperion/AssetImport/SceneImport.h"
#include "Hyperion/IO/Path.h"
#include <set>

namespace Hyperion
{
namespace
{
void Import(std::span<const std::string_view> InArguments, FIOService& InIO, std::ostream& InOutput)
{
	if (InArguments.size() < 3)
	{
		throw std::invalid_argument(
		    "import/upgrade SOURCE OUTPUT.hasset [--scene] [--force] [--name NAME] [--type ID] [--library DIRECTORY]");
	}
	FAssetImportOptions Options;
	for (std::size_t Index = 3; Index < InArguments.size(); ++Index)
	{
		const auto Argument = InArguments[Index];
		if (Argument == "--scene")
		{
			Options.bScene = true;
		}
		else if (Argument == "--force")
		{
			Options.bForce = true;
		}
		else if (Argument == "--library" && Index + 1 < InArguments.size())
		{
			Options.Library = PathFromUtf8(InArguments[++Index]);
		}
		else if (Argument == "--source-root" && Index + 1 < InArguments.size())
		{
			Options.SourceRoot = PathFromUtf8(InArguments[++Index]);
		}
		else if (Argument == "--source-id" && Index + 1 < InArguments.size())
		{
			Options.SourceId = InArguments[++Index];
		}
		else if ((Argument == "--name" || Argument == "--type") && Index + 1 < InArguments.size())
		{
			auto& Value = Argument == "--name" ? Options.Name : Options.TypeId;
			Value = InArguments[++Index];
		}
		else
		{
			throw std::invalid_argument("Unknown or incomplete import option: " + std::string(Argument));
		}
	}
	FAssetImportService Imports(InIO);
	RegisterGltfImporter(Imports);
	RegisterSceneImporter(Imports);
	const auto Result =
	    Imports.ImportAsync(PathFromUtf8(InArguments[1]), PathFromUtf8(InArguments[2]), Options).Get(InIO.TaskSystem());
	InOutput << (Result->bUpToDate ? "Up to date: " : "Published: ") << PathToUtf8(Result->Output)
	         << "\nid=" << Result->Header.Id << " type=" << Result->Header.TypeId
	         << " schema=" << Result->Header.SchemaVersion << " revision=" << Result->Header.Revision
	         << " written_assets=" << Result->WrittenAssets << '\n';
}

void Inspect(const FLoadedAsset& InAsset, std::ostream& InOutput)
{
	const auto& Header = InAsset.Header;
	InOutput << "path=" << PathToUtf8(InAsset.Path) << "\nid=" << Header.Id << " type=" << Header.TypeId
	         << " schema=" << Header.SchemaVersion << " revision=" << Header.Revision
	         << " dependencies=" << Header.Dependencies.size() << '\n';
	for (const auto& Dependency : Header.Dependencies)
	{
		InOutput << "  " << Dependency.Field << " -> " << Dependency.Reference.Path << " id=" << Dependency.Reference.Id
		         << " revision=" << Dependency.Reference.Revision << '\n';
	}
	for (const auto& Diagnostic : InAsset.Diagnostics)
	{
		InOutput << "  diagnostic: " << Diagnostic << '\n';
	}
	if (Header.Import)
	{
		InOutput << "importer=" << Header.Import->Importer << " version=" << Header.Import->ImporterVersion << '\n';
		for (const auto& Source : Header.Import->Sources)
		{
			InOutput << "  source=" << Source.Path << " fingerprint=" << Source.Fingerprint << '\n';
		}
	}
	if (InAsset.Type->CppType == typeid(FSceneManifest))
	{
		InOutput << "instances=" << SceneModelCount(*InAsset.As<FSceneManifest>()) << '\n';
	}
}

std::shared_ptr<const FAssetGraph> Validate(FAssetService& InAssets, FTaskSystem& InTasks,
                                            const std::filesystem::path& InPath)
{
	const auto Graph = InAssets.LoadGraphAsync(InPath).Get(InTasks);
	if (!Graph->Failures.empty())
	{
		const auto& Failure = Graph->Failures.front();
		throw std::runtime_error(PathToUtf8(Failure.Parent) + ":" + Failure.Field + ": " + Failure.Error);
	}
	return Graph;
}

void ExportJson(std::span<const std::string_view> InArguments, FAssetService& InAssets, FIOService& InIO)
{
	if (InArguments.size() != 3)
	{
		throw std::invalid_argument("export-json INPUT.hasset OUTPUT.json");
	}
	const auto Asset = InAssets.LoadAsync(PathFromUtf8(InArguments[1])).Get(InIO.TaskSystem());
	const auto Output = InAssets.NormalizePath(PathFromUtf8(InArguments[2]));
	auto Object = ReadRecord(*Asset->Type, WriteRecord(*Asset->Type, Asset->Object.get()));
	VisitRecord(*Asset->Type, Object.get(),
	            [&](const FRecordDescriptor& InType, const void* InValue, std::string_view)
	            {
		            if (InType.CppType == typeid(FAssetRef))
		            {
			            auto& Reference = *const_cast<FAssetRef*>(static_cast<const FAssetRef*>(InValue));
			            if (!Reference.Path.empty())
			            {
				            const auto Absolute = InAssets.Resolve(Reference, Asset->Path);
				            Reference.Path = PathRelativeToUtf8(Absolute, Output.parent_path());
			            }
		            }
	            });
	const auto Text = EncodeAssetSourceJson(WriteRecord(*Asset->Type, Object.get()));
	const auto Bytes = std::as_bytes(std::span(Text));
	InIO.WriteAsync(Output, FBytes(Bytes.begin(), Bytes.end())).Get(InIO.TaskSystem());
}

void Catalog(std::span<const std::string_view> InArguments, FAssetService& InAssets, FTaskSystem& InTasks,
             std::ostream& InOutput)
{
	if (InArguments.size() < 3)
	{
		throw std::invalid_argument("catalog OUTPUT.hasset ROOT.hasset [ROOT.hasset ...]");
	}
	const auto Output = InAssets.NormalizePath(PathFromUtf8(InArguments[1]));
	FAssetCatalog CatalogValue;
	std::map<std::string, FAssetRef> Unique;
	for (std::size_t Index = 2; Index < InArguments.size(); ++Index)
	{
		const auto Graph = Validate(InAssets, InTasks, PathFromUtf8(InArguments[Index]));
		for (const auto& [AssetPath, Asset] : Graph->Assets)
		{
			FAssetRef Reference{Asset->Header.Id, PathRelativeToUtf8(AssetPath, Output.parent_path()),
			                    Asset->Header.TypeId, Asset->Header.Revision};
			const auto [It, bInserted] = Unique.emplace(Reference.Id, Reference);
			if (!bInserted && It->second != Reference)
			{
				throw std::runtime_error("Conflicting catalog identity: " + Reference.Id);
			}
		}
	}
	for (const auto& [Id, Reference] : Unique)
	{
		CatalogValue.Assets.push_back(Reference);
	}
	InAssets.SaveAsync(Output, std::make_shared<const FAssetCatalog>(std::move(CatalogValue))).Get(InTasks);
	InOutput << "Catalog: " << PathToUtf8(Output) << " assets=" << Unique.size() << '\n';
}
} // namespace

void RunAssetCommand(std::span<const std::string_view> InArguments, FIOService& InIO, std::ostream& InOutput)
{
	if (InArguments.empty() || InArguments[0] == "--help")
	{
		InOutput << "hyperion_asset_tool import SOURCE OUTPUT.hasset [--scene] [--force] [--name NAME] [--type ID] "
		            "[--library DIRECTORY] [--source-root DIRECTORY --source-id ID]\n"
		         << "Global options: --mounts CONFIG.json [--authoring]\n"
		         << "hyperion_asset_tool build-brdf OUTPUT.hasset\n"
		         << "hyperion_asset_tool upgrade LEGACY.hasset OUTPUT.hasset\n"
		         << "hyperion_asset_tool inspect|validate ROOT.hasset\n"
		         << "hyperion_asset_tool export-json INPUT.hasset OUTPUT.json\n"
		         << "hyperion_asset_tool export-envelope INPUT.hasset OUTPUT.json\n"
		         << "hyperion_asset_tool measure-source SOURCE.gltf|SOURCE.glb\n"
		         << "hyperion_asset_tool catalog OUTPUT.hasset ROOT.hasset [ROOT.hasset ...]\n";
		return;
	}
	const auto Command = InArguments[0];
	if (Command == "export-envelope" && InArguments.size() == 3)
	{
		const auto Document = DecodeAsset(InIO.ReadAsync(PathFromUtf8(InArguments[1])).Get(InIO.TaskSystem()));
		const auto Text = EncodeAssetSourceJson(Document.Object);
		const auto Bytes = std::as_bytes(std::span(Text));
		InIO.WriteAsync(PathFromUtf8(InArguments[2]), FBytes(Bytes.begin(), Bytes.end())).Get(InIO.TaskSystem());
		return;
	}
	if (Command == "measure-source" && InArguments.size() == 2)
	{
		FAssetImportService Imports(InIO);
		RegisterGltfImporter(Imports);
		const auto Model = Imports.LoadAsync<FModelAsset>(PathFromUtf8(InArguments[1])).Get(InIO.TaskSystem());
		if (Model->Primitives.empty() || ModelInstances(*Model).empty())
		{
			throw std::runtime_error("Cannot measure an empty model");
		}
		InOutput << "CPU model ready: primitives=" << Model->Primitives.size()
		         << " instances=" << ModelInstances(*Model).size() << '\n';
		return;
	}
	if (Command == "import" || Command == "upgrade")
	{
		Import(InArguments, InIO, InOutput);
		return;
	}
	FAssetService Assets(InIO);
	RegisterSceneAssetTypes(Assets.Types());
	Assets.Types().Register<FAssetCatalog>();
	if (Command == "build-brdf" && InArguments.size() == 2)
	{
		const auto Brdf = BuildEnvironmentBrdf();
		FAssetHeader Header;
		Header.Id = "00000000000000000000000000000001";
		const auto Encoded = EncodeAsset(RecordType<FTextureAsset>(), &Brdf, Header);
		InIO.WriteAsync(PathFromUtf8(InArguments[1]), Encoded.Bytes).Get(InIO.TaskSystem());
	}
	else if (Command == "export-json")
	{
		ExportJson(InArguments, Assets, InIO);
	}
	else if (Command == "catalog")
	{
		Catalog(InArguments, Assets, InIO.TaskSystem(), InOutput);
	}
	else if ((Command == "inspect" || Command == "validate") && InArguments.size() == 2)
	{
		const auto Graph = Validate(Assets, InIO.TaskSystem(), PathFromUtf8(InArguments[1]));
		Inspect(*Graph->Root, InOutput);
		InOutput << "Validated native graph: " << Graph->Assets.size() << " assets\n";
	}
	else
	{
		throw std::invalid_argument("Unknown command or incorrect arguments; use --help");
	}
}
} // namespace Hyperion
