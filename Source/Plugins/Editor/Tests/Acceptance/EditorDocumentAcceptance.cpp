#include "EditorAcceptanceHarness.h"
#include "Hyperion/Renderer/SceneNavigation.h"

namespace Hyperion
{
namespace
{
void Check(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

template<class T> void Reject(const T& InOperation)
{
	bool bRejected{};
	try
	{
		InOperation();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	Check(bRejected, "Editor accepted an invalid or stale document operation");
}
} // namespace

void FEditorAcceptanceHarness::ExerciseDocumentInput(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Scene->GetStatus().Error.empty())
	{
		throw std::runtime_error(Editor.Scene->GetStatus().Error);
	}
	if (!Editor.Selection || !Editor.Scene->GetStatus().bReady || Editor.ReadyFrames < 8)
	{
		return;
	}
	const auto MeshType = RecordType<FSceneModelComponent>().Id;
	switch (Scenario.Document.Progress.GetState())
	{
		case EDocumentState::ExpandModelComponent:
			Scenario.ExerciseOriginal = *Editor.Scene->FindNode(*Editor.Selection);
			if (Scenario.InspectionBounds.contains(RecordType<FSceneModelSource>().Id + "/header"))
			{
				if (ExerciseClick(InEvents,
				                  Scenario.InspectionBounds.at(RecordType<FSceneModelSource>().Id + "/header"),
				                  Scenario.Document.Click))
				{
					Scenario.Document.Progress.TransitionTo(EDocumentState::ExerciseTransformAndExpandComponent);
				}
			}
			else
			{
				Scenario.Document.Progress.TransitionTo(EDocumentState::ExerciseTransformAndExpandComponent);
			}
			break;
		case EDocumentState::ExerciseTransformAndExpandComponent:
			if (!ExerciseTransformInput(InEvents))
			{
				break;
			}
			if (ExerciseClick(InEvents, Scenario.InspectionBounds.at(RecordType<FSceneTransform>().Id + "/header"),
			                  Scenario.Document.Click))
			{
				Scenario.Document.Progress.TransitionTo(EDocumentState::HideModel);
			}
			break;
		case EDocumentState::HideModel:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds.at(MeshType + "/visible"), Scenario.Document.Click))
			{
				Scenario.Document.Progress.TransitionTo(EDocumentState::VerifyHiddenModel);
			}
			break;
		case EDocumentState::VerifyHiddenModel:
			if (!Editor.IsDirty() || Editor.Scene->FindNode(*Editor.Selection)->Model()->bVisible)
			{
				const auto Bounds = Scenario.InspectionBounds.at(MeshType + "/visible");
				throw std::runtime_error(
				    "Inspector change was not applied immediately; target=" + std::to_string(Bounds.X) + "," +
				    std::to_string(Bounds.Y) + "," + std::to_string(Bounds.Z) + "," + std::to_string(Bounds.W));
			}
			Check(Editor.HistoryCursor == 1 && Editor.History.size() == 1,
			      "New editing did not truncate the redo branch");
			Scenario.Document.Progress.TransitionTo(EDocumentState::SaveEditedDocument);
			break;
		case EDocumentState::SaveEditedDocument:
		{
			Check(Editor.IsDirty() && !Editor.Scene->FindNode(*Editor.Selection)->Model()->bVisible,
			      "Live inspector did not commit the component");
			const auto Readback = Editor.Scene->GetComponentDiagnostics(*Editor.Selection, MeshType);
			if (!Readback || !Readback->bApplied)
			{
				break;
			}
			Check(!Readback->Primitives.empty() && !Readback->Primitives.front().bAppliedVisible,
			      "Rendering diagnostics did not observe the committed visibility");
			Editor.Undo();
			Check(!Editor.IsDirty() && Editor.Scene->FindNode(*Editor.Selection)->Model()->bVisible,
			      "Undo lost the save point");
			Editor.Redo();
			auto Transformed = *Editor.Scene->FindNode(*Editor.Selection);
			Transformed.Local() = Scenario.ExerciseTransformResult;
			Editor.CommitEdit(*Editor.Selection, std::move(Transformed), Editor.Scene->GetRevision());
			Editor.SaveScene(Editor.Options.ExerciseDocument.string());
			auto Candidate = *Editor.Scene->FindNode(*Editor.Selection);
			Candidate.Name += " unsaved";
			Editor.CommitEdit(*Editor.Selection, std::move(Candidate), Editor.Scene->GetRevision());
			Scenario.Document.Progress.TransitionTo(EDocumentState::AwaitDocumentSave);
			break;
		}
		case EDocumentState::AwaitDocumentSave:
			if (Editor.PendingSave)
			{
				break;
			}
			Check(Editor.LastSaveMilliseconds > 0 && Editor.IsDirty(),
			      "Save completion incorrectly cleared a later edit");
			Editor.Undo();
			Check(!Editor.IsDirty(), "Undo did not return to the captured save state");
			Editor.OpenScene(Editor.Options.ExerciseDocument.string());
			Scenario.Document.Progress.TransitionTo(EDocumentState::VerifyReloadAndAttemptFailedSave);
			break;
		case EDocumentState::VerifyReloadAndAttemptFailedSave:
		{
			const auto Handle = Editor.Scene->FindHandle(Scenario.ExerciseOriginal.Id);
			const auto* Node = Editor.Scene->FindNode(Handle);
			Check(Node && !Node->Model()->bVisible && Node->Name == Scenario.ExerciseOriginal.Name,
			      "Saved component state did not survive reload");
			Check(Node->Local().Values == Scenario.ExerciseTransformResult.Values,
			      "Transform display edit did not survive native scene save/reload");
			const auto Revision = Editor.Scene->GetRevision();
			auto Invalid = *Node;
			Invalid.Local().Values[15] = 0;
			Reject(
			    [&]
			    {
				    Editor.CommitEdit(Handle, Invalid, Revision);
			    });
			Reject(
			    [&]
			    {
				    Editor.CommitEdit(Handle, *Node, Revision - 1);
			    });
			const auto PreviousCamera = Editor.Viewport.ViewCamera;
			DollySceneCamera(Editor.Viewport.ViewCamera, .9f);
			Check(Editor.Scene->GetRevision() == Revision && !Editor.IsDirty(),
			      "Viewport camera changed authored data");
			Editor.Viewport.ViewCamera = PreviousCamera;
			auto Candidate = *Node;
			Candidate.Name += " rejected save";
			Editor.CommitEdit(Handle, std::move(Candidate), Revision);
			Editor.SaveScene("/Engine/Scenes/ReadOnlySaveMustFail.hasset");
			Scenario.Document.Progress.TransitionTo(EDocumentState::AwaitFailedDocumentSave);
			break;
		}
		case EDocumentState::AwaitFailedDocumentSave:
			if (Editor.PendingSave)
			{
				break;
			}
			Check(Editor.IsDirty() && Editor.Error.starts_with("Save failed:"), "Failed save lost the dirty document");
			Editor.Undo();
			Check(!Editor.IsDirty(), "Failed save advanced the save point");
			Editor.Error.clear();
			Scenario.bDocumentVerified = true;
			break;
	}
}
} // namespace Hyperion
