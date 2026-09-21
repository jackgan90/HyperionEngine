#include "EditorApplication.h"
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

void FEditorPlugin::ExerciseDocumentInput(std::vector<FInputEvent>& InEvents)
{
	if (!Scene->GetStatus().Error.empty())
	{
		throw std::runtime_error(Scene->GetStatus().Error);
	}
	if (!Selection || !Scene->GetStatus().bReady || ReadyFrames < 8)
	{
		return;
	}
	const auto MeshType = RecordType<FSceneModelComponent>().Id;
	switch (ExerciseStep)
	{
		case 0:
			ExerciseOriginal = *Scene->FindNode(*Selection);
			if (InspectionBounds.contains(RecordType<FSceneModelSource>().Id + "/header"))
			{
				ExerciseClick(InEvents, InspectionBounds.at(RecordType<FSceneModelSource>().Id + "/header"));
			}
			else
			{
				++ExerciseStep;
			}
			break;
		case 1:
			if (!ExerciseTransformInput(InEvents))
			{
				break;
			}
			ExerciseClick(InEvents, InspectionBounds.at(RecordType<FSceneTransform>().Id + "/header"));
			break;
		case 2:
			ExerciseClick(InEvents, InspectionBounds.at(MeshType + "/visible"));
			break;
		case 3:
			if (!IsDirty() || Scene->FindNode(*Selection)->Model()->bVisible)
			{
				const auto Bounds = InspectionBounds.at(MeshType + "/visible");
				throw std::runtime_error(
				    "Inspector change was not applied immediately; target=" + std::to_string(Bounds.X) + "," +
				    std::to_string(Bounds.Y) + "," + std::to_string(Bounds.Z) + "," + std::to_string(Bounds.W));
			}
			Check(HistoryCursor == 1 && History.size() == 1, "New editing did not truncate the redo branch");
			++ExerciseStep;
			break;
		case 4:
		{
			Check(IsDirty() && !Scene->FindNode(*Selection)->Model()->bVisible,
			      "Live inspector did not commit the component");
			const auto Readback = Scene->GetComponentDiagnostics(*Selection, MeshType);
			if (!Readback || !Readback->bApplied)
			{
				break;
			}
			Check(!Readback->Primitives.empty() && !Readback->Primitives.front().bAppliedVisible,
			      "Rendering diagnostics did not observe the committed visibility");
			Undo();
			Check(!IsDirty() && Scene->FindNode(*Selection)->Model()->bVisible, "Undo lost the save point");
			Redo();
			auto Transformed = *Scene->FindNode(*Selection);
			Transformed.Local() = ExerciseTransformResult;
			CommitEdit(*Selection, std::move(Transformed), Scene->GetRevision());
			SaveScene(Options.ExerciseDocument.string());
			auto Candidate = *Scene->FindNode(*Selection);
			Candidate.Name += " unsaved";
			CommitEdit(*Selection, std::move(Candidate), Scene->GetRevision());
			++ExerciseStep;
			break;
		}
		case 5:
			if (PendingSave)
			{
				break;
			}
			Check(LastSaveMilliseconds > 0 && IsDirty(), "Save completion incorrectly cleared a later edit");
			Undo();
			Check(!IsDirty(), "Undo did not return to the captured save state");
			OpenScene(Options.ExerciseDocument.string());
			++ExerciseStep;
			break;
		case 6:
		{
			const auto Handle = Scene->FindHandle(ExerciseOriginal.Id);
			const auto* Node = Scene->FindNode(Handle);
			Check(Node && !Node->Model()->bVisible && Node->Name == ExerciseOriginal.Name,
			      "Saved component state did not survive reload");
			Check(Node->Local().Values == ExerciseTransformResult.Values,
			      "Transform display edit did not survive native scene save/reload");
			const auto Revision = Scene->GetRevision();
			auto Invalid = *Node;
			Invalid.Local().Values[15] = 0;
			Reject(
			    [&]
			    {
				    CommitEdit(Handle, Invalid, Revision);
			    });
			Reject(
			    [&]
			    {
				    CommitEdit(Handle, *Node, Revision - 1);
			    });
			const auto PreviousCamera = ViewCamera;
			DollySceneCamera(ViewCamera, .9f);
			Check(Scene->GetRevision() == Revision && !IsDirty(), "Viewport camera changed authored data");
			ViewCamera = PreviousCamera;
			auto Candidate = *Node;
			Candidate.Name += " rejected save";
			CommitEdit(Handle, std::move(Candidate), Revision);
			SaveScene("/Engine/Scenes/ReadOnlySaveMustFail.hasset");
			++ExerciseStep;
			break;
		}
		case 7:
			if (PendingSave)
			{
				break;
			}
			Check(IsDirty() && Error.starts_with("Save failed:"), "Failed save lost the dirty document");
			Undo();
			Check(!IsDirty(), "Failed save advanced the save point");
			Error.clear();
			bDocumentVerified = true;
			break;
	}
}
} // namespace Hyperion
