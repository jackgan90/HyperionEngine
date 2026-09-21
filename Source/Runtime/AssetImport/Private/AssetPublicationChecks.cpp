#include "AssetPublicationInternal.h"
#include "Hyperion/AssetImport/MaterialImport.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Scene/SceneManifest.h"

namespace Hyperion
{
void FPublication::Prepare(const FAssetImportOptions& InOptions)
{
	const auto Extension = ImportExtension(Source);
	SourceType = InOptions.TypeId;
	if (SourceType.empty() && Extension == ".hasset")
	{
		SourceType = DecodeAsset(IO.ReadAsync(Source, Cancellation).Get(IO.TaskSystem())).Header.TypeId;
	}
	if (SourceType.empty() && Extension == ".json")
	{
		const auto SourceBytes = IO.ReadAsync(Source, Cancellation).Get(IO.TaskSystem());
		const auto Node =
		    DecodeAssetSourceJson({reinterpret_cast<const char*>(SourceBytes->data()), SourceBytes->size()});
		const auto& Object = std::get<FArchiveNode::FObject>(Node.Value);
		if (const auto It = Object.find("type"); It != Object.end())
		{
			SourceType = ReadValue<std::string>(It->second);
		}
	}
	for (const auto& Importer : Importers)
	{
		Provenance.Settings["importer:" + Importer.Id] = std::to_string(Importer.Version);
		if (SourceType.empty() &&
		    std::find(Importer.Extensions.begin(), Importer.Extensions.end(), Extension) != Importer.Extensions.end())
		{
			SourceType = Importer.Type->Id;
		}
	}
	if (SourceType.empty())
	{
		throw std::invalid_argument("Cannot infer source type; specify --type");
	}
	const auto Importer = std::find_if(Importers.begin(), Importers.end(),
	                                   [&](const FAssetImporter& InImporter)
	                                   {
		                                   return InImporter.Type->Id == SourceType &&
		                                          std::find(InImporter.Extensions.begin(), InImporter.Extensions.end(),
		                                                    Extension) != InImporter.Extensions.end();
	                                   });
	if (Importer == Importers.end())
	{
		throw std::invalid_argument("No matching source importer");
	}
	if (InOptions.bScene && SourceType != RecordType<FModelAsset>().Id && SourceType != RecordType<FSceneManifest>().Id)
	{
		throw std::invalid_argument("--scene requires a model or scene source");
	}
	Provenance.Importer = Importer->Id;
	Provenance.ImporterVersion = Importer->Version;
	Provenance.Settings["type"] = SourceType;
	Provenance.Settings["source"] = ImportPathString(Source.filename());
	Provenance.Settings["scene"] = InOptions.bScene ? "true" : "false";
	if (InOptions.bScene || SourceType == RecordType<FSceneManifest>().Id)
	{
		// Publication policy also applies to --scene with only the model importer registered.
		Provenance.Settings["scene_model_policy"] = "whole-model-reference";
		Provenance.Settings["scene_view_policy"] = "independent-browsing-view";
	}
	Provenance.Settings["name"] = InOptions.Name;
	Provenance.Settings["library"] = ImportRelativePath(Library, Output.parent_path());
	const auto Existing = IO.TryReadAsync(Output, Cancellation).Get(IO.TaskSystem());
	if (*Existing)
	{
		// A damaged output can be rebuilt; permission/IO failures above remain errors.
		try
		{
			Previous = DecodeAsset(**Existing);
		}
		catch (const std::runtime_error&)
		{
		}
	}
	RootId = Previous ? Previous->Header.Id : CreateIdentifier();
	const auto OutputType = InOptions.bScene ? RecordType<FSceneManifest>().Id : SourceType;
	if (Previous && Previous->Header.TypeId != OutputType)
	{
		throw std::invalid_argument("Cannot reimport a different type over an existing native asset");
	}
	if (SourceId.empty())
	{
		SourceRoot = Source.parent_path();
		SourceId = "asset/" + RootId;
	}
	Provenance.Settings["source_id"] = StableSourceKey(Source);
	Provenance.Settings["publication"] = "current-native-v1";
	if (Previous && Previous->Header.Import)
	{
		for (const auto& [Key, Id] : Previous->Header.Import->OutputIds)
		{
			PreviousIds.emplace(PortableKey(Key), Id);
		}
	}
}

bool FPublication::SourcesCurrent(const FAssetGraph& InGraph) const
{
	std::set<std::string> NativeFingerprints;
	bool bReadNativeFingerprints{};
	for (const auto& Entry : Previous->Header.Import->Sources)
	{
		const auto EntryPath = PathFromUtf8(Entry.Path);
		if (IsPackagePath(EntryPath) && ImportExtension(EntryPath) == ".hasset")
		{
			if (!bReadNativeFingerprints)
			{
				for (const auto& [Path, Asset] : InGraph.Assets)
				{
					if (Path != Output)
					{
						NativeFingerprints.insert(ContentHash(*IO.ReadAsync(Path, Cancellation).Get(IO.TaskSystem())));
					}
				}
				bReadNativeFingerprints = true;
			}
			// Full container fingerprints include identity. A renamed native dependency remains
			// the same source even when its old path hint has been reused or renamed repeatedly.
			if (NativeFingerprints.contains(Entry.Fingerprint))
			{
				continue;
			}
		}
		const auto Path = IsPackagePath(EntryPath) ? EntryPath : Source.parent_path() / EntryPath;
		if (ContentHash(*IO.ReadAsync(Path, Cancellation).Get(IO.TaskSystem())) != Entry.Fingerprint)
		{
			return false;
		}
	}
	return true;
}

bool FPublication::IsCurrent()
{
	if (!Previous || !Previous->Header.Import)
	{
		return false;
	}
	const auto& Old = *Previous->Header.Import;
	if (Old.Importer != Provenance.Importer || Old.ImporterVersion != Provenance.ImporterVersion ||
	    Old.Settings != Provenance.Settings || Old.Sources.empty())
	{
		return false;
	}
	try
	{
		auto& Assets = IndexedAssets();
		const auto Graph = Assets.LoadGraphAsync(Output).Get(IO.TaskSystem());
		if (!Graph->Failures.empty())
		{
			return false;
		}
		for (const auto& [Path, Asset] : Graph->Assets)
		{
			if (!Asset->Diagnostics.empty() || Asset->Header.SchemaVersion != Asset->Type->Version)
			{
				return false;
			}
		}
		return SourcesCurrent(*Graph);
	}
	catch (const std::runtime_error&)
	{
		Cancellation.Check();
		return false;
	}
}

void FPublication::Track(const FConvertedAsset& InAsset)
{
	for (const auto& Entry : InAsset.Sources)
	{
		const auto Path = ImportPath(PathFromUtf8(Entry.Path));
		const auto [It, bInserted] = Sources.emplace(Path, Entry.Fingerprint);
		if (!bInserted && It->second != Entry.Fingerprint)
		{
			throw std::runtime_error("Source changed during dependency conversion: " + Entry.Path);
		}
	}
}

void FPublication::CheckSources() const
{
	for (const auto& [Path, Fingerprint] : Sources)
	{
		if (ContentHash(*IO.ReadAsync(Path, Cancellation).Get(IO.TaskSystem())) != Fingerprint)
		{
			throw std::runtime_error("Source changed before publication: " + ImportPathString(Path));
		}
	}
}

void FPublication::Write(const std::filesystem::path& InPath, const FEncodedAsset& InAsset)
{
	Cancellation.Check();
	Staged[InPath] = InAsset;
}
} // namespace Hyperion
