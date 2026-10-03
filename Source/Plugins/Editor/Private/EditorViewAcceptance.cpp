#include "EditorAcceptanceHarness.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <cmath>

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

bool Near(const FMat4& InA, const FMat4& InB)
{
	for (std::size_t Index = 0; Index < 16; ++Index)
	{
		if (std::abs(InA.Values[Index] - InB.Values[Index]) > .001f)
		{
			return false;
		}
	}
	return true;
}
} // namespace

void FEditorAcceptanceHarness::ExerciseViewHistory()
{
	Check(Editor.Selection && Editor.Scene->FindNode(*Editor.Selection)->Camera(), "Create camera widget failed");
	Scenario.ExerciseOriginal = *Editor.Scene->FindNode(*Editor.Selection);
	const auto FirstHandle = *Editor.Selection;
	Editor.Undo();
	Check(!Editor.Scene->FindNode(FirstHandle), "Undo creation left the camera in the scene");
	Editor.Redo();
	Check(*Editor.Selection != FirstHandle &&
	          Editor.Scene->FindNode(*Editor.Selection)->Id == Scenario.ExerciseOriginal.Id,
	      "Redo did not recreate the camera safely");
	FSceneNode Parent;
	Parent.Id = "view-acceptance-parent";
	Parent.Local() = Translation({8, -3, 1});
	const auto ParentHandle = Editor.Scene->AddNode(Parent);
	auto Child = *Editor.Scene->FindNode(*Editor.Selection);
	Child.Parent() = Parent.Id;
	Editor.CommitEdit(*Editor.Selection, Child, Editor.Scene->GetRevision());
	Editor.ApplyEditorView(*Editor.Selection);
	FSceneNodeView View;
	Check(Editor.Scene->GetNodeView(*Editor.Selection, View) && Near(View.World, Editor.Viewport.ViewCamera.World),
	      "Applying world view ignored the camera parent");
	Editor.Undo();
	Editor.Undo();
	Editor.Redo();
	Editor.Redo();
	Check(Editor.Scene->GetNodeView(*Editor.Selection, View) && Near(View.World, Editor.Viewport.ViewCamera.World),
	      "Redo lost parent-relative camera transform");
	Editor.Undo();
	Editor.Undo();
	Parent.Local().Values[0] = 0;
	Editor.Scene->EditNode(ParentHandle, Parent, Editor.Scene->GetRevision());
	Child = *Editor.Scene->FindNode(*Editor.Selection);
	Child.Parent() = Parent.Id;
	Editor.CommitEdit(*Editor.Selection, Child, Editor.Scene->GetRevision());
	const auto Revision = Editor.Scene->GetRevision();
	bool bRejected{};
	try
	{
		Editor.ApplyEditorView(*Editor.Selection);
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	Check(bRejected && Editor.Scene->GetRevision() == Revision, "Singular parent edit was not rejected atomically");
	Editor.Undo();
	Editor.Scene->RemoveSubtree(ParentHandle);
	DollySceneCamera(Editor.Viewport.ViewCamera, .8f);
	Scenario.ExerciseEditorView = Editor.Viewport.ViewCamera;
}

void FEditorAcceptanceHarness::ExerciseViewPreview(std::vector<FInputEvent>& InEvents)
{
	const auto Handle = Editor.Scene->FindHandle(Scenario.ExerciseOriginal.Id);
	switch (Scenario.ExerciseStep)
	{
		case 3:
			Check(Editor.Scene->FindNode(Handle)->Local().Values == Editor.Viewport.ViewCamera.World.Values,
			      "Apply editor view widget failed");
			Editor.Undo();
			Check(Editor.Scene->FindNode(Handle)->Local().Values == Scenario.ExerciseOriginal.Local().Values,
			      "Undo camera view failed");
			Editor.Redo();
			ExerciseClick(InEvents, Scenario.InspectionBounds.at("view/preview"));
			break;
		case 4:
		{
			Check(Editor.Viewport.PreviewCamera == Handle && Editor.IsPreviewAvailable(),
			      "Preview camera widget failed");
			auto Node = *Editor.Scene->FindNode(Handle);
			Node.bEnabled = false;
			Editor.CommitEdit(Handle, Node, Editor.Scene->GetRevision());
			++Scenario.ExerciseStep;
			break;
		}
		case 5:
		{
			Check(!Editor.IsPreviewAvailable() && Editor.RenderStats.MainView().Draws == 0,
			      "Disabled preview still rendered scene geometry");
			auto Node = *Editor.Scene->FindNode(Handle);
			Node.Camera().reset();
			Node.bEnabled = true;
			Editor.CommitEdit(Handle, Node, Editor.Scene->GetRevision());
			++Scenario.ExerciseStep;
			break;
		}
		case 6:
			if (Scenario.ExerciseWait == 0)
			{
				Check(!Editor.IsPreviewAvailable() && Editor.RenderStats.MainView().Draws == 0,
				      "Missing Camera component fell back to another camera");
				Editor.Undo();
				Editor.Undo();
				Check(Editor.IsPreviewAvailable(), "Undo did not restore camera preview");
			}
			ExerciseClick(InEvents, Scenario.InspectionBounds.at("view/return"));
			break;
		case 7:
			Check(!Editor.Viewport.PreviewCamera && Editor.Viewport.ViewCamera == Scenario.ExerciseEditorView,
			      "Returning from preview lost the editor view");
			Check(Editor.Scene->GetSettings().InitialView == Scenario.ExerciseInitialView,
			      "Camera edit changed the initial view");
			Editor.SaveScene(Editor.Options.ExerciseViews.string());
			++Scenario.ExerciseStep;
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseViewInput(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Scene->GetStatus().Error.empty())
	{
		throw std::runtime_error(Editor.Scene->GetStatus().Error);
	}
	if (!Editor.Scene->GetStatus().bReady || !Editor.Viewport.bViewportCameraInitialized || Editor.ReadyFrames < 8)
	{
		return;
	}
	if ((Scenario.ExerciseStep == 0 || Scenario.ExerciseStep == 1 || Scenario.ExerciseStep == 6) &&
	    !Editor.bViewOptionsOpen)
	{
		const auto Step = Scenario.ExerciseStep;
		ExerciseClick(InEvents, Scenario.InspectionBounds.at("view/options"));
		Scenario.ExerciseStep = Step;
		return;
	}
	if (Scenario.ExerciseStep >= 3 && Scenario.ExerciseStep <= 7)
	{
		ExerciseViewPreview(InEvents);
		return;
	}
	switch (Scenario.ExerciseStep)
	{
		case 0:
			if (Scenario.ExerciseWait == 0)
			{
				const auto Revision = Editor.Scene->GetRevision();
				DollySceneCamera(Editor.Viewport.ViewCamera, .9f);
				Check(!Editor.IsDirty() && Editor.Scene->GetRevision() == Revision,
				      "Navigation changed authored scene data");
				Scenario.ExerciseInitialView = Editor.Viewport.ViewCamera;
			}
			ExerciseClick(InEvents, Scenario.InspectionBounds.at("view/initial"));
			break;
		case 1:
			Check(Editor.IsDirty() && Editor.Scene->GetSettings().InitialView == Scenario.ExerciseInitialView,
			      "Set initial view widget failed");
			Editor.Undo();
			Check(!Editor.IsDirty(), "Initial view undo lost save point");
			Editor.Redo();
			ExerciseClick(InEvents, Scenario.InspectionBounds.at("view/create"));
			break;
		case 2:
			if (Scenario.ExerciseWait == 0)
			{
				ExerciseViewHistory();
			}
			ExerciseClick(InEvents, Scenario.InspectionBounds.at("view/apply"));
			break;
		case 8:
			if (!Editor.PendingSave)
			{
				Check(!Editor.IsDirty(), "View document save failed");
				Editor.OpenScene(Editor.Options.ExerciseViews.string());
				++Scenario.ExerciseStep;
			}
			break;
		case 9:
		{
			Check(Editor.Viewport.ViewCamera == Scenario.ExerciseInitialView && !Editor.IsDirty(),
			      "Opening scene did not restore the explicit initial view");
			const auto Handle = Editor.Scene->FindHandle(Scenario.ExerciseOriginal.Id);
			const auto* Node = Editor.Scene->FindNode(Handle);
			Check(Node && Node->Camera() && Node->Local().Values == Scenario.ExerciseEditorView.World.Values,
			      "Authored camera did not survive save/reload");
			Editor.SetPreviewCamera(Handle);
			Editor.Scene->RemoveSubtree(Handle);
			Check(!Editor.IsPreviewAvailable(), "Deleted camera preview remained available");
			++Scenario.ExerciseStep;
			break;
		}
		case 10:
			Check(Editor.RenderStats.MainView().Draws == 0, "Deleted preview still rendered scene geometry");
			Editor.SetPreviewCamera({});
			Scenario.bViewsVerified = true;
			break;
	}
}
} // namespace Hyperion
