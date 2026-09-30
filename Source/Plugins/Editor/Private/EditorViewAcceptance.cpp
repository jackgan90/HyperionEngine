#include "EditorApplication.h"
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

void FEditorPlugin::ExerciseViewHistory()
{
	Check(Selection && Scene->FindNode(*Selection)->Camera(), "Create camera widget failed");
	Acceptance.ExerciseOriginal = *Scene->FindNode(*Selection);
	const auto FirstHandle = *Selection;
	Undo();
	Check(!Scene->FindNode(FirstHandle), "Undo creation left the camera in the scene");
	Redo();
	Check(*Selection != FirstHandle && Scene->FindNode(*Selection)->Id == Acceptance.ExerciseOriginal.Id,
	      "Redo did not recreate the camera safely");
	FSceneNode Parent;
	Parent.Id = "view-acceptance-parent";
	Parent.Local() = Translation({8, -3, 1});
	const auto ParentHandle = Scene->AddNode(Parent);
	auto Child = *Scene->FindNode(*Selection);
	Child.Parent() = Parent.Id;
	CommitEdit(*Selection, Child, Scene->GetRevision());
	ApplyEditorView(*Selection);
	FSceneNodeView View;
	Check(Scene->GetNodeView(*Selection, View) && Near(View.World, Viewport.ViewCamera.World),
	      "Applying world view ignored the camera parent");
	Undo();
	Undo();
	Redo();
	Redo();
	Check(Scene->GetNodeView(*Selection, View) && Near(View.World, Viewport.ViewCamera.World),
	      "Redo lost parent-relative camera transform");
	Undo();
	Undo();
	Parent.Local().Values[0] = 0;
	Scene->EditNode(ParentHandle, Parent, Scene->GetRevision());
	Child = *Scene->FindNode(*Selection);
	Child.Parent() = Parent.Id;
	CommitEdit(*Selection, Child, Scene->GetRevision());
	const auto Revision = Scene->GetRevision();
	bool bRejected{};
	try
	{
		ApplyEditorView(*Selection);
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	Check(bRejected && Scene->GetRevision() == Revision, "Singular parent edit was not rejected atomically");
	Undo();
	Scene->RemoveSubtree(ParentHandle);
	DollySceneCamera(Viewport.ViewCamera, .8f);
	Acceptance.ExerciseEditorView = Viewport.ViewCamera;
}

void FEditorPlugin::ExerciseViewPreview(std::vector<FInputEvent>& InEvents)
{
	const auto Handle = Scene->FindHandle(Acceptance.ExerciseOriginal.Id);
	switch (Acceptance.ExerciseStep)
	{
		case 3:
			Check(Scene->FindNode(Handle)->Local().Values == Viewport.ViewCamera.World.Values,
			      "Apply editor view widget failed");
			Undo();
			Check(Scene->FindNode(Handle)->Local().Values == Acceptance.ExerciseOriginal.Local().Values,
			      "Undo camera view failed");
			Redo();
			ExerciseClick(InEvents, InspectionBounds.at("view/preview"));
			break;
		case 4:
		{
			Check(Viewport.PreviewCamera == Handle && IsPreviewAvailable(), "Preview camera widget failed");
			auto Node = *Scene->FindNode(Handle);
			Node.bEnabled = false;
			CommitEdit(Handle, Node, Scene->GetRevision());
			++Acceptance.ExerciseStep;
			break;
		}
		case 5:
		{
			Check(!IsPreviewAvailable() && RenderStats.MainView().Draws == 0,
			      "Disabled preview still rendered scene geometry");
			auto Node = *Scene->FindNode(Handle);
			Node.Camera().reset();
			Node.bEnabled = true;
			CommitEdit(Handle, Node, Scene->GetRevision());
			++Acceptance.ExerciseStep;
			break;
		}
		case 6:
			if (Acceptance.ExerciseWait == 0)
			{
				Check(!IsPreviewAvailable() && RenderStats.MainView().Draws == 0,
				      "Missing Camera component fell back to another camera");
				Undo();
				Undo();
				Check(IsPreviewAvailable(), "Undo did not restore camera preview");
			}
			ExerciseClick(InEvents, InspectionBounds.at("view/return"));
			break;
		case 7:
			Check(!Viewport.PreviewCamera && Viewport.ViewCamera == Acceptance.ExerciseEditorView,
			      "Returning from preview lost the editor view");
			Check(Scene->GetSettings().InitialView == Acceptance.ExerciseInitialView,
			      "Camera edit changed the initial view");
			SaveScene(Options.ExerciseViews.string());
			++Acceptance.ExerciseStep;
			break;
	}
}

void FEditorPlugin::ExerciseViewInput(std::vector<FInputEvent>& InEvents)
{
	if (!Scene->GetStatus().Error.empty())
	{
		throw std::runtime_error(Scene->GetStatus().Error);
	}
	if (!Scene->GetStatus().bReady || !Viewport.bViewportCameraInitialized || ReadyFrames < 8)
	{
		return;
	}
	if ((Acceptance.ExerciseStep == 0 || Acceptance.ExerciseStep == 1 || Acceptance.ExerciseStep == 6) &&
	    !bViewOptionsOpen)
	{
		const auto Step = Acceptance.ExerciseStep;
		ExerciseClick(InEvents, InspectionBounds.at("view/options"));
		Acceptance.ExerciseStep = Step;
		return;
	}
	if (Acceptance.ExerciseStep >= 3 && Acceptance.ExerciseStep <= 7)
	{
		ExerciseViewPreview(InEvents);
		return;
	}
	switch (Acceptance.ExerciseStep)
	{
		case 0:
			if (Acceptance.ExerciseWait == 0)
			{
				const auto Revision = Scene->GetRevision();
				DollySceneCamera(Viewport.ViewCamera, .9f);
				Check(!IsDirty() && Scene->GetRevision() == Revision, "Navigation changed authored scene data");
				Acceptance.ExerciseInitialView = Viewport.ViewCamera;
			}
			ExerciseClick(InEvents, InspectionBounds.at("view/initial"));
			break;
		case 1:
			Check(IsDirty() && Scene->GetSettings().InitialView == Acceptance.ExerciseInitialView,
			      "Set initial view widget failed");
			Undo();
			Check(!IsDirty(), "Initial view undo lost save point");
			Redo();
			ExerciseClick(InEvents, InspectionBounds.at("view/create"));
			break;
		case 2:
			if (Acceptance.ExerciseWait == 0)
			{
				ExerciseViewHistory();
			}
			ExerciseClick(InEvents, InspectionBounds.at("view/apply"));
			break;
		case 8:
			if (!PendingSave)
			{
				Check(!IsDirty(), "View document save failed");
				OpenScene(Options.ExerciseViews.string());
				++Acceptance.ExerciseStep;
			}
			break;
		case 9:
		{
			Check(Viewport.ViewCamera == Acceptance.ExerciseInitialView && !IsDirty(),
			      "Opening scene did not restore the explicit initial view");
			const auto Handle = Scene->FindHandle(Acceptance.ExerciseOriginal.Id);
			const auto* Node = Scene->FindNode(Handle);
			Check(Node && Node->Camera() && Node->Local().Values == Acceptance.ExerciseEditorView.World.Values,
			      "Authored camera did not survive save/reload");
			SetPreviewCamera(Handle);
			Scene->RemoveSubtree(Handle);
			Check(!IsPreviewAvailable(), "Deleted camera preview remained available");
			++Acceptance.ExerciseStep;
			break;
		}
		case 10:
			Check(RenderStats.MainView().Draws == 0, "Deleted preview still rendered scene geometry");
			SetPreviewCamera({});
			Acceptance.bViewsVerified = true;
			break;
	}
}
} // namespace Hyperion
