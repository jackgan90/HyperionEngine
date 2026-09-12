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
	if (Previous && Previous->Header.Import)
	{
		PreviousIds = Previous->Header.Import->OutputIds;
	}
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
		for (const auto& Entry : Old.Sources)
		{
			const auto Path = Source.parent_path() / PathFromUtf8(Entry.Path);
			if (ContentHash(*IO.ReadAsync(Path, Cancellation).Get(IO.TaskSystem())) != Entry.Fingerprint)
			{
				return false;
			}
		}
		FAssetService Assets(IO);
		for (const auto& Importer : Importers)
		{
			Assets.Types().Register(*Importer.Type);
		}
		Assets.Types().Register<FSceneManifest>();
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
		return true;
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
	const auto Existing = IO.TryReadAsync(InPath, Cancellation).Get(IO.TaskSystem());
	if (*Existing && **Existing == InAsset.Bytes)
	{
		return;
	}
	IO.WriteAsync(InPath, InAsset.Bytes, Cancellation).Get(IO.TaskSystem());
	++Written;
}
} // namespace Hyperion
