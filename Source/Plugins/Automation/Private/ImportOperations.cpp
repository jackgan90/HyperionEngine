#include "ImportOperations.h"
#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/SceneImport.h"
#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
namespace
{
struct FImportRequest
{
	std::uint64_t Generation{};
	std::string Source;
	std::string Output;
	std::string Library;
	std::string Name;
	std::string Type;
	std::string SourceRoot;
	std::string SourceId;
	bool bScene{};
	bool bForce{};
};

struct FImportResult
{
	FAssetRef Asset;
	std::uint64_t WrittenAssets{};
	bool bUpToDate{};
};
} // namespace

template<> const FRecordDescriptor& RecordType<FImportRequest>()
{
	static const auto Type = MakeRecord<FImportRequest>(
	    "automation.asset.import",
	    {Member("generation", &FImportRequest::Generation,
	            {.bRequired = true, .Description = "Current content.root.get generation."}),
	     Member("source", &FImportRequest::Source,
	            {.bRequired = true, .Description = "Source path accessible to the target process."}),
	     Member(
	         "output", &FImportRequest::Output,
	         {.bRequired = true, .Description = "Separate native .hasset destination. Mount write permissions apply."}),
	     Member("library", &FImportRequest::Library,
	            {.Description = "Shared dependency publication directory; defaults to output parent."}),
	     Member("name", &FImportRequest::Name), Member("type", &FImportRequest::Type),
	     Member("sourceRoot", &FImportRequest::SourceRoot), Member("sourceId", &FImportRequest::SourceId),
	     Member("scene", &FImportRequest::bScene), Member("force", &FImportRequest::bForce)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImportResult>()
{
	static const auto Type = MakeRecord<FImportResult>("automation.asset.import.result",
	                                                   {Member("asset", &FImportResult::Asset),
	                                                    Member("writtenAssets", &FImportResult::WrittenAssets),
	                                                    Member("upToDate", &FImportResult::bUpToDate)});
	return Type;
}

FImportAutomation::FImportAutomation(FIOService& InIO, FAssetService& InAssets, FContentRootService& InRoots)
    : Imports(InIO), Assets(InAssets), Roots(InRoots)
{
	RegisterGltfImporter(Imports);
	RegisterSceneImporter(Imports);
}

void FImportAutomation::Register(FOperationCatalog& InCatalog)
{
	FOperationInfo Info;
	Info.Id = "asset.import";
	Info.Owner = "automation-assets";
	Info.Summary = "Import source assets and publish native dependency products";
	Info.Description = "Uses AssetTool's production import/publication service: glTF/GLB (including referenced "
	                   "images), HDR/EXR sky environments, typed JSON, sky recipes and native upgrades. Standalone "
	                   "PNG/JPEG import is not registered. Optional type disambiguates JSON. scene wraps a model as a "
	                   "scene. sourceRoot/sourceId must be supplied together. force bypasses up-to-date detection, not "
	                   "write permission or identity checks. Existing open drafts are not silently reloaded.";
	Info.Effects = "Writes the destination and shared dependency library with native publication leases; refreshes the "
	               "target asset index. No scene history.";
	Info.Completion = "All native publications committed and index refreshed. Accepted publication is not cancellable.";
	const FImportRequest Example{1, "source.gltf", "/Game/Imported.hasset"};
	Info.Example = WriteRecordWire(RecordType<FImportRequest>(), &Example);
	InCatalog.Register(MakeAsyncOperation<FImportRequest, FImportResult>(
	    Info,
	    [this](const FImportRequest& InRequest)
	    {
		    if (InRequest.Generation != Roots.Info().Generation)
		    {
			    throw FAutomationError("stale_revision", "Content root changed before import");
		    }
		    FAssetImportOptions Options;
		    Options.bScene = InRequest.bScene;
		    Options.bForce = InRequest.bForce;
		    Options.Name = InRequest.Name;
		    Options.TypeId = InRequest.Type;
		    Options.Library = PathFromUtf8(InRequest.Library);
		    Options.SourceRoot = PathFromUtf8(InRequest.SourceRoot);
		    Options.SourceId = InRequest.SourceId;
		    auto Pending = Imports.ImportAsync(PathFromUtf8(InRequest.Source), PathFromUtf8(InRequest.Output), Options);
		    ++ActiveJobs;
		    return TPendingOperation<FImportResult>{
		        [this, Pending]() -> std::optional<FImportResult>
		        {
			        if (!Pending.Ready())
			        {
				        return {};
			        }
			        --ActiveJobs;
			        const auto Result = Pending.GetReady();
			        Assets.ClearCache();
			        IndexDiscoveredAssets(Assets, Result->Output.parent_path());
			        return FImportResult{
			            {Result->Header.Id, PathToUtf8(Result->Output), Result->Header.TypeId, Result->Header.Revision},
			            Result->WrittenAssets,
			            Result->bUpToDate};
		        }};
	    }));
}

void FImportAutomation::Drain()
{
	Imports.Drain();
}

FContentRootParticipantState FImportAutomation::ContentRootState() const
{
	return {false, ActiveJobs != 0};
}

void FImportAutomation::ReleaseContentRoot()
{
	Imports.ClearCache();
}

void FImportAutomation::ContentRootChanged()
{
}
} // namespace Hyperion
