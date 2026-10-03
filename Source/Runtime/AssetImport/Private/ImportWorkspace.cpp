#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/SkyImport.h"
#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/IO/Path.h"
#include <algorithm>

namespace Hyperion
{
FAssetImportWorkspace::FAssetImportWorkspace(FIOService& InIO, FAssetService& InAssets, FContentRootService& InRoots)
    : IO(InIO), Assets(InAssets), Roots(InRoots), Imports(InIO)
{
	RegisterGltfImporter(Imports);
	RegisterSkyImporter(Imports);
}

FAssetImportWorkspace::~FAssetImportWorkspace()
{
	Drain();
}

void FAssetImportWorkspace::RequireMain() const
{
	if (std::this_thread::get_id() != Owner)
	{
		throw std::logic_error("Import workspace is Main-only");
	}
}

FImportCapabilities FAssetImportWorkspace::Capabilities()
{
	return {{{RecordType<FModelAsset>().Id,
	          {".gltf", ".glb"},
	          "Static model with generated material/texture assets; optional scene wrapper."},
	         {RecordType<FTextureAsset>().Id,
	          {".png", ".jpg", ".jpeg"},
	          "Standalone PNG/JPEG to RGBA8 with full mips; optional textureEncoding."},
	         {RecordType<FSkyAsset>().Id, {".hdr", ".exr"}, "2:1 HDR/EXR panorama with sky bake settings."}},
	        false};
}

std::shared_ptr<const FImportTask> FAssetImportWorkspace::Start(const FImportRequest& InRequest)
{
	return StartPrepared(InRequest, {}, {});
}

FAssetImportOptions FAssetImportWorkspace::Options(const FImportRequest& InRequest) const
{
	const auto Validated = Validate(InRequest);
	FAssetImportOptions Result;
	Result.Name = InRequest.Name;
	Result.TypeId = InRequest.Type;
	Result.Library = PathFromUtf8(Validated.Library);
	Result.SourceRoot = PathFromUtf8(InRequest.SourceRoot);
	Result.SourceId = InRequest.SourceId;
	Result.RootId = InRequest.RootId;
	Result.bScene = InRequest.bScene;
	Result.bForce = InRequest.bForce;
	Result.bCreateFolder = InRequest.bCreateFolder;
	Result.Conversion = {InRequest.TextureEncoding, InRequest.Sky};
	return Result;
}

std::shared_ptr<const FImportTask> FAssetImportWorkspace::StartPrepared(
    const FImportRequest& InRequest, std::shared_ptr<const FPreparedImport> InPrepared, std::string InOverrides)
{
	RequireMain();
	if (bClosing)
	{
		throw FAssetImportError("unavailable", "Import workspace is closing");
	}
	const auto Validated = Validate(InRequest);
	if (Tasks.size() >= 128)
	{
		const auto Oldest = std::find_if(Tasks.begin(), Tasks.end(),
		                                 [](const auto& InTask)
		                                 {
			                                 return InTask->Info.Status != EImportTaskState::Running;
		                                 });
		if (Oldest == Tasks.end())
		{
			throw FAssetImportError("busy", "Import task capacity reached; wait for a task to complete");
		}
		Tasks.erase(Oldest);
	}
	auto ImportOptions = Options(InRequest);
	ImportOptions.Prepared = std::move(InPrepared);
	ImportOptions.PropertyOverrides = std::move(InOverrides);
	auto Task = std::make_shared<FImportTask>();
	Task->Info = {CreateIdentifier(), InRequest.Generation, Validated.Source, Validated.Output};
	// Allocate the tracking slot before admitting work; no accepted producer can become untracked.
	Tasks.push_back(Task);
	try
	{
		Task->Pending =
		    Imports.ImportAsync(PathFromUtf8(Validated.Source), PathFromUtf8(Validated.Output), ImportOptions);
	}
	catch (...)
	{
		Tasks.pop_back();
		throw;
	}
	Log(ELogLevel::Debug, "Import accepted; task='" + Task->Info.Task + "'; source='" + Task->Info.Source +
	                          "'; output='" + Task->Info.Output +
	                          "'; generation=" + std::to_string(Task->Info.Generation));
	return Task;
}

void FAssetImportWorkspace::Complete(FImportTask& InTask)
{
	try
	{
		const auto Result = InTask.Pending.GetReady();
		FImportResult Outcome{
		    {Result->Header.Id, PathToUtf8(Result->Output), Result->Header.TypeId, Result->Header.Revision},
		    Result->WrittenAssets,
		    Result->bUpToDate,
		    InTask.Info.Task};
		try
		{
			Assets.ClearCache();
			IndexDiscoveredAssets(Assets, Result->Output.parent_path());
		}
		catch (const std::exception& Failure)
		{
			Outcome.Warning = std::string("Publication committed; index refresh failed: ") + Failure.what();
			Outcome.Warning.resize(std::min<std::size_t>(Outcome.Warning.size(), 8192));
		}
		InTask.Info.Result = std::move(Outcome);
		InTask.Info.Output = PathToUtf8(Result->Output);
		InTask.Info.Status = EImportTaskState::Completed;
		++ContentRevision;
	}
	catch (const std::exception& Failure)
	{
		InTask.Info.Status = EImportTaskState::Failed;
		InTask.Info.Error = Failure.what();
		InTask.Info.Error.resize(std::min<std::size_t>(InTask.Info.Error.size(), 8192));
	}
	InTask.Pending = {};
	const auto& Info = InTask.Info;
	const std::string Context = "task='" + Info.Task + "'; source='" + Info.Source + "'; output='" + Info.Output +
	                            "'; generation=" + std::to_string(Info.Generation);
	if (Info.Result)
	{
		Log(ELogLevel::Info, "Import completed; " + Context + "; asset='" + Info.Result->Asset.Id +
		                         "'; written_assets=" + std::to_string(Info.Result->WrittenAssets) +
		                         "; up_to_date=" + (Info.Result->bUpToDate ? "true" : "false"));
		if (!Info.Result->Warning.empty())
		{
			Log(ELogLevel::Warning, "Import completed with warning; " + Context + "; reason=" + Info.Result->Warning);
		}
	}
	else
	{
		Log(ELogLevel::Error, "Import failed; " + Context + "; reason=" + Info.Error);
	}
}

void FAssetImportWorkspace::Update()
{
	RequireMain();
	for (const auto& Task : Tasks)
	{
		if (Task->Info.Status == EImportTaskState::Running && Task->Pending.Ready())
		{
			Complete(*Task);
		}
	}
	UpdateDrafts();
}

void FAssetImportWorkspace::Drain()
{
	RequireMain();
	if (!bClosing)
	{
		bClosing = true;
		Imports.Drain();
		Update();
	}
}

std::uint64_t FAssetImportWorkspace::Revision() const
{
	RequireMain();
	return ContentRevision;
}

FContentRootParticipantState FAssetImportWorkspace::ContentRootState() const
{
	RequireMain();
	const bool bDirty =
	    std::any_of(Drafts.begin(), Drafts.end(),
	                [](const auto& InDraft)
	                {
		                return InDraft->Status == EImportDraftState::Ready &&
		                       ImportPropertiesKey(InDraft->History[InDraft->Cursor]) != InDraft->SavedKey;
	                });
	const bool bPreparing = std::any_of(Drafts.begin(), Drafts.end(),
	                                    [](const auto& InDraft)
	                                    {
		                                    return InDraft->Status == EImportDraftState::Preparing;
	                                    });
	return {bDirty, bPreparing || std::any_of(Tasks.begin(), Tasks.end(),
	                                          [](const auto& InTask)
	                                          {
		                                          return InTask->Info.Status == EImportTaskState::Running;
	                                          })};
}

void FAssetImportWorkspace::ReleaseContentRoot()
{
	RequireMain();
	Imports.ClearCache();
	Tasks.clear();
	Drafts.clear();
}

void FAssetImportWorkspace::ContentRootChanged()
{
	RequireMain();
	++ContentRevision;
}

FImportTaskInfo FAssetImportWorkspace::Get(const FImportTaskQuery& InRequest) const
{
	RequireMain();
	for (const auto& Task : Tasks)
	{
		if (Task->Info.Task == InRequest.Task)
		{
			return Task->Info;
		}
	}
	throw FAssetImportError("not_found", "Import task expired or belongs to another workspace/root");
}

FImportTaskList FAssetImportWorkspace::List(const FImportTaskListRequest& InRequest) const
{
	RequireMain();
	if (!InRequest.Limit || InRequest.Limit > 32)
	{
		throw FAssetImportError("invalid_arguments", "Import task page limit must be 1-32");
	}
	FImportTaskList Result;
	Result.Total = static_cast<std::uint32_t>(Tasks.size());
	for (std::size_t Index = InRequest.Offset; Index < Tasks.size() && Result.Tasks.size() < InRequest.Limit; ++Index)
	{
		Result.Tasks.push_back(Tasks[Tasks.size() - 1 - Index]->Info);
	}
	return Result;
}
} // namespace Hyperion
