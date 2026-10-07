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
	switch (Scenario.View.Progress.GetState())
	{
		case EViewState::VerifyViewAndPreviewCamera:
			Check(Editor.Scene->FindNode(Handle)->Local().Values == Editor.Viewport.ViewCamera.World.Values,
			      "Apply editor view widget failed");
			Editor.Undo();
			Check(Editor.Scene->FindNode(Handle)->Local().Values == Scenario.ExerciseOriginal.Local().Values,
			      "Undo camera view failed");
			Editor.Redo();
			if (ExerciseClick(InEvents, Scenario.Bounds.Require(EEditorWidget::PreviewCamera), Scenario.View.Click))
			{
				Scenario.View.Progress.TransitionTo(EViewState::DisablePreviewCamera);
			}
			break;
		case EViewState::DisablePreviewCamera:
		{
			Check(Editor.Viewport.PreviewCamera == Handle && Editor.IsPreviewAvailable(),
			      "Preview camera widget failed");
			auto Node = *Editor.Scene->FindNode(Handle);
			Node.bEnabled = false;
			Editor.CommitEdit(Handle, Node, Editor.Scene->GetRevision());
			Scenario.View.Progress.TransitionTo(EViewState::RemovePreviewCameraComponent);
			break;
		}
		case EViewState::RemovePreviewCameraComponent:
		{
			Check(!Editor.IsPreviewAvailable() && Editor.RenderStats.MainView().Draws == 0,
			      "Disabled preview still rendered scene geometry");
			auto Node = *Editor.Scene->FindNode(Handle);
			Node.Camera().reset();
			Node.bEnabled = true;
			Editor.CommitEdit(Handle, Node, Editor.Scene->GetRevision());
			Scenario.View.Progress.TransitionTo(EViewState::RestoreCameraAndReturnToEditor);
			break;
		}
		case EViewState::RestoreCameraAndReturnToEditor:
			if (!Scenario.View.bCameraRestored)
			{
				Scenario.View.bCameraRestored = true;
				Check(!Editor.IsPreviewAvailable() && Editor.RenderStats.MainView().Draws == 0,
				      "Missing Camera component fell back to another camera");
				Editor.Undo();
				Editor.Undo();
				Check(Editor.IsPreviewAvailable(), "Undo did not restore camera preview");
			}
			if (ExerciseClick(InEvents, Scenario.Bounds.Require(EEditorWidget::ReturnToEditorView),
			                  Scenario.View.Click))
			{
				Scenario.View.Progress.TransitionTo(EViewState::SaveViewDocument);
			}
			break;
		case EViewState::SaveViewDocument:
			Check(!Editor.Viewport.PreviewCamera && Editor.Viewport.ViewCamera == Scenario.ExerciseEditorView,
			      "Returning from preview lost the editor view");
			Check(Editor.Scene->GetSettings().InitialView == Scenario.ExerciseInitialView,
			      "Camera edit changed the initial view");
			Editor.SaveScene(Editor.Options.ExerciseViews.string());
			Scenario.View.Progress.TransitionTo(EViewState::AwaitViewSave);
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
	if ((Scenario.View.Progress.Is(EViewState::SetInitialView) || Scenario.View.Progress.Is(EViewState::CreateCamera) ||
	     Scenario.View.Progress.Is(EViewState::RestoreCameraAndReturnToEditor)) &&
	    !Editor.bViewOptionsOpen)
	{
		ExerciseClick(InEvents, Scenario.Bounds.Require(EEditorWidget::ViewOptions), Scenario.View.Click);
		return;
	}
	if (Scenario.View.Progress.IsAny({EViewState::VerifyViewAndPreviewCamera, EViewState::DisablePreviewCamera,
	                                  EViewState::RemovePreviewCameraComponent,
	                                  EViewState::RestoreCameraAndReturnToEditor, EViewState::SaveViewDocument}))
	{
		ExerciseViewPreview(InEvents);
		return;
	}
	switch (Scenario.View.Progress.GetState())
	{
		case EViewState::SetInitialView:
			if (!Scenario.View.bInitialViewCaptured)
			{
				Scenario.View.bInitialViewCaptured = true;
				const auto Revision = Editor.Scene->GetRevision();
				DollySceneCamera(Editor.Viewport.ViewCamera, .9f);
				Check(!Editor.IsDirty() && Editor.Scene->GetRevision() == Revision,
				      "Navigation changed authored scene data");
				Scenario.ExerciseInitialView = Editor.Viewport.ViewCamera;
			}
			if (ExerciseClick(InEvents, Scenario.Bounds.Require(EEditorWidget::InitialView), Scenario.View.Click))
			{
				Scenario.View.Progress.TransitionTo(EViewState::CreateCamera);
			}
			break;
		case EViewState::CreateCamera:
			Check(Editor.IsDirty() && Editor.Scene->GetSettings().InitialView == Scenario.ExerciseInitialView,
			      "Set initial view widget failed");
			Editor.Undo();
			Check(!Editor.IsDirty(), "Initial view undo lost save point");
			Editor.Redo();
			if (ExerciseClick(InEvents, Scenario.Bounds.Require(EEditorWidget::CreateCamera), Scenario.View.Click))
			{
				Scenario.View.Progress.TransitionTo(EViewState::ApplyEditorView);
			}
			break;
		case EViewState::ApplyEditorView:
			if (!Scenario.View.bApplyBaselineCaptured)
			{
				Scenario.View.bApplyBaselineCaptured = true;
				ExerciseViewHistory();
			}
			if (ExerciseClick(InEvents, Scenario.Bounds.Require(EEditorWidget::ApplyCamera), Scenario.View.Click))
			{
				Scenario.View.Progress.TransitionTo(EViewState::VerifyViewAndPreviewCamera);
			}
			break;
		case EViewState::AwaitViewSave:
			if (!Editor.PendingSave)
			{
				Check(!Editor.IsDirty(), "View document save failed");
				Editor.OpenScene(Editor.Options.ExerciseViews.string());
				Scenario.View.Progress.TransitionTo(EViewState::VerifyReloadAndDeleteCamera);
			}
			break;
		case EViewState::VerifyReloadAndDeleteCamera:
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
			Scenario.View.Progress.TransitionTo(EViewState::VerifyDeletedPreview);
			break;
		}
		case EViewState::VerifyDeletedPreview:
			Check(Editor.RenderStats.MainView().Draws == 0, "Deleted preview still rendered scene geometry");
			Editor.SetPreviewCamera({});
			Scenario.bViewsVerified = true;
			break;
	}
}
} // namespace Hyperion
