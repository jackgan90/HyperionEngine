#include "ImportOperations.h"

namespace Hyperion
{
namespace
{
FOperationInfo ImportInfo(std::string InId, std::string InSummary, std::string InDescription, FArchiveNode InExample,
                          bool bInReadOnly, const FAssetImportWorkspace* InProvider)
{
	return {std::move(InId),
	        std::move(InSummary),
	        std::move(InDescription),
	        "automation-assets",
	        bInReadOnly
	            ? "Reads import state; no publication."
	            : "Publishes native assets; refreshes the index without replacing open drafts. No scene history.",
	        bInReadOnly
	            ? "Main snapshot returned."
	            : "Publication committed; warning identifies any subsequent index refresh failure. Not cancellable.",
	        std::move(InExample),
	        1,
	        bInReadOnly,
	        {"asset", "import", "texture", "sky", "gltf"},
	        InProvider ? "" : "Import workspace is unavailable; enable assets with a valid Engine content directory"};
}

template<class T> FArchiveNode Example(const T& InValue)
{
	return WriteRecordWire(RecordType<T>(), &InValue);
}
} // namespace

void RegisterImportOperations(FOperationCatalog& InCatalog, FAssetImportWorkspace* InProvider)
{
	RegisterImportDraftOperations(InCatalog, InProvider);
	const FImportRequest Request{1, "source.gltf", "/Game/Imported.hasset"};
	InCatalog.Register(MakeAsyncOperation<FImportRequest, FImportResult>(
	    ImportInfo(
	        "asset.import", "Import source assets and publish native dependency products",
	        "Shared Editor/AssetTool pipeline: glTF/GLB, standalone PNG/JPEG, HDR/EXR skies. "
	        "sourceRoot/sourceId must be supplied together. force bypasses freshness, not identity or permissions. "
	        "Optional textureEncoding applies to standalone images; sky specifies HDR/EXR bake settings. Use "
	        "asset.import.tasks for application task IDs.",
	        Example(Request), false, InProvider),
	    [InProvider](const FImportRequest& InRequest)
	    {
		    const auto Task = InProvider->Start(InRequest);
		    return TPendingOperation<FImportResult>{[InProvider, Task]() -> std::optional<FImportResult>
		                                            {
			                                            InProvider->Update();
			                                            if (Task->Info.Status == EImportTaskState::Running)
			                                            {
				                                            return {};
			                                            }
			                                            if (Task->Info.Status == EImportTaskState::Failed)
			                                            {
				                                            throw FAutomationError(AutomationErrors::OperationFailed,
				                                                                   Task->Info.Error);
			                                            }
			                                            return Task->Info.Result;
		                                            }};
	    }));
	InCatalog.Register(MakeOperation<FContentRootQuery, FImportCapabilities>(
	    ImportInfo("asset.import.capabilities", "Describe supported import formats",
	               "Supported output types and external source extensions. Inspect asset.import for typed settings. No "
	               "accepted "
	               "task cancellation.",
	               Example(FContentRootQuery{}), true, InProvider),
	    [](const FContentRootQuery&)
	    {
		    return FAssetImportWorkspace::Capabilities();
	    }));
	InCatalog.Register(MakeOperation<FImportRequest, FImportValidation>(
	    ImportInfo("asset.import.validate", "Validate an import request without publication",
	               "Checks generation, paths, supported options and mount permissions. Conversion and "
	               "dependency/identity checks still run on import; successful validation is not a reservation.",
	               Example(Request), true, InProvider),
	    [InProvider](const FImportRequest& InRequest)
	    {
		    return InProvider->Validate(InRequest);
	    }));
	InCatalog.Register(MakeOperation<FImportTaskListRequest, FImportTaskList>(
	    ImportInfo("asset.import.tasks", "List GUI and agent import tasks",
	               "Newest first, limit 1-32. Up to 128 retained application tasks; root changes clear history. IDs "
	               "are distinct from session jobs.",
	               Example(FImportTaskListRequest{}), true, InProvider),
	    [InProvider](const FImportTaskListRequest& InRequest)
	    {
		    return InProvider->List(InRequest);
	    }));
	InCatalog.Register(MakeOperation<FImportTaskQuery, FImportTaskInfo>(
	    ImportInfo("asset.import.task", "Inspect a shared import task",
	               "Query an application import task ID returned by asset.import.tasks, including tasks submitted in "
	               "the GUI or another connection.",
	               Example(FImportTaskQuery{"task-from-asset.import.tasks"}), true, InProvider),
	    [InProvider](const FImportTaskQuery& InRequest)
	    {
		    return InProvider->Get(InRequest);
	    }));
}
} // namespace Hyperion
