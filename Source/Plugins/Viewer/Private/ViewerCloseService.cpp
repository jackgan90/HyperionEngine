#include "Hyperion/SceneEditing/SceneDocument.h"
#include "ViewerApplication.h"

namespace Hyperion
{
FApplicationCloseState FViewerPlugin::ApplicationCloseState() const
{
	const auto* Document = Context->Find<FSceneEditDocument>();
	return {CloseState, CloseError, Document && Document->IsDirty(),
	        CloseSave.has_value() || (Document && (Document->IsBusy() || Document->GetState().Save.has_value())),
	        Document ? Document->GetState().Path : std::string{}};
}

FApplicationCloseState FViewerPlugin::RequestApplicationClose(const FApplicationCloseRequest& InRequest)
{
	if (InRequest.Action == EApplicationCloseAction::Cancel)
	{
		bCloseAfterSave = false;
		CloseState = "idle";
		CloseError.clear();
		Window->CancelClose();
		return ApplicationCloseState();
	}
	const auto State = ApplicationCloseState();
	if (State.bBusy || bFinished || bStopped)
	{
		throw FSceneEditError("busy", "Wait for pending scene interactions or saves");
	}
	if (State.bDirty && InRequest.Action == EApplicationCloseAction::RejectDirty)
	{
		throw FSceneEditError("dirty_document", "Save first or explicitly select save/discard");
	}
	CloseError.clear();
	if (State.bDirty && InRequest.Action == EApplicationCloseAction::Save)
	{
		auto& Document = *Context->Find<FSceneEditDocument>();
		const auto Path = InRequest.ScenePath.empty() ? State.ScenePath : InRequest.ScenePath;
		if (Path.empty())
		{
			throw std::invalid_argument("Supply scenePath for an untitled scene");
		}
		Document.RequireIdle(Document.Id(), Document.Target().Revision());
		CloseSave = Document.Save(Path);
		bCloseAfterSave = true;
		CloseState = "saving";
	}
	else
	{
		CloseState = "closing";
		Window->RequestClose();
	}
	return ApplicationCloseState();
}

void FViewerPlugin::PollApplicationClose()
{
	if (!CloseSave || !CloseSave->Ready())
	{
		return;
	}
	try
	{
		CloseSave->GetReady();
		auto& Document = *Context->Find<FSceneEditDocument>();
		Document.PollSave();
		if (bCloseAfterSave)
		{
			if (Document.IsDirty())
			{
				throw std::runtime_error("Scene changed during save; application remains open");
			}
			CloseState = "closing";
			Window->RequestClose();
		}
	}
	catch (const std::exception& Failure)
	{
		CloseState = "failed";
		CloseError = Failure.what();
	}
	CloseSave.reset();
	bCloseAfterSave = false;
}
} // namespace Hyperion
