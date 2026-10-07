#include "EditorAcceptanceHarness.h"
#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/IO/Path.h"
#include <array>

namespace Hyperion
{
namespace
{
enum class ESceneLifecycleDecision
{
	Cancel,
	Dismiss,
	CancelSavePath,
	Discard,
	SaveAs,
	SaveNamed
};

struct FSceneLifecycleCase
{
	ESceneDocumentAction Action;
	ESceneLifecycleDecision Decision;
};

constexpr std::array Cases{FSceneLifecycleCase{ESceneDocumentAction::New, ESceneLifecycleDecision::Cancel},
                           FSceneLifecycleCase{ESceneDocumentAction::New, ESceneLifecycleDecision::Dismiss},
                           FSceneLifecycleCase{ESceneDocumentAction::New, ESceneLifecycleDecision::CancelSavePath},
                           FSceneLifecycleCase{ESceneDocumentAction::New, ESceneLifecycleDecision::Discard},
                           FSceneLifecycleCase{ESceneDocumentAction::New, ESceneLifecycleDecision::SaveAs},
                           FSceneLifecycleCase{ESceneDocumentAction::New, ESceneLifecycleDecision::SaveNamed},
                           FSceneLifecycleCase{ESceneDocumentAction::Close, ESceneLifecycleDecision::Cancel},
                           FSceneLifecycleCase{ESceneDocumentAction::Close, ESceneLifecycleDecision::Dismiss},
                           FSceneLifecycleCase{ESceneDocumentAction::Close, ESceneLifecycleDecision::CancelSavePath},
                           FSceneLifecycleCase{ESceneDocumentAction::Close, ESceneLifecycleDecision::Discard},
                           FSceneLifecycleCase{ESceneDocumentAction::Close, ESceneLifecycleDecision::SaveAs},
                           FSceneLifecycleCase{ESceneDocumentAction::Close, ESceneLifecycleDecision::SaveNamed}};

void Check(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

void CheckSavedScene(IFileSystem& InFileSystem, const std::filesystem::path& InPath, std::size_t InNodes)
{
	const auto Asset = DecodeAsset(InFileSystem.Read(InPath, 1024 * 1024));
	const auto Manifest =
	    std::static_pointer_cast<FSceneManifest>(ReadRecord(RecordType<FSceneManifest>(), Asset.Object));
	Check(Manifest->Nodes.size() == InNodes, "GUI scene save did not persist the expected objects");
}
} // namespace

void FEditorAcceptanceHarness::PrepareSceneLifecycleCase()
{
	auto& Context = Scenario.SceneLifecycle;
	switch (Context.Progress.GetState())
	{
		case ESceneLifecycleState::PrepareCase:
		{
			const auto Current = Editor.DocumentStatus().Scene;
			Context.Fixture = Editor.ChangeDocument(ESceneDocumentAction::New,
			                                        {Current.Document, Current.Revision, ESceneDirtyAction::Discard});
			Context.Destination =
			    Editor.Options.ExerciseSceneLifecycle / ("Case" + std::to_string(Context.CaseIndex) + ".hasset");
			std::filesystem::remove(Context.Destination);
			Context.ReadySettle.Restart();
			Context.Progress.TransitionTo(ESceneLifecycleState::AwaitFixture);
			break;
		}
		case ESceneLifecycleState::AwaitFixture:
			if (Context.Fixture->Failure)
			{
				std::rethrow_exception(Context.Fixture->Failure);
			}
			if (!Context.Fixture->Result || Context.ReadySettle.ConsumeFrame())
			{
				break;
			}
			Context.Fixture.reset();
			Context.OriginalHandle =
			    CreateSceneNode(Editor.SceneDocument,
			                    {Editor.SceneDocument.Id(), Editor.Scene->GetRevision(), "Lifecycle fixture"})
			        .Handle;
			Context.OriginalDocument = Editor.SceneDocument.Id();
			if (Cases[Context.CaseIndex].Decision == ESceneLifecycleDecision::SaveNamed)
			{
				Editor.SaveScene(PathToUtf8(Context.Destination));
				Context.Progress.TransitionTo(ESceneLifecycleState::AwaitFixtureSave);
			}
			else
			{
				Context.Progress.TransitionTo(ESceneLifecycleState::OpenFileMenu);
			}
			break;
		case ESceneLifecycleState::AwaitFixtureSave:
			if (!Editor.PendingSave)
			{
				auto Node = *Editor.Scene->FindNode(Context.OriginalHandle);
				Node.Name += " edited";
				Editor.CommitEdit(Context.OriginalHandle, std::move(Node), Editor.Scene->GetRevision());
				Context.Progress.TransitionTo(ESceneLifecycleState::OpenFileMenu);
			}
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseSceneLifecycleDecision(std::vector<FInputEvent>& InEvents)
{
	auto& Context = Scenario.SceneLifecycle;
	const auto Decision = Cases[Context.CaseIndex].Decision;
	FVec4 Bounds;
	if (Decision == ESceneLifecycleDecision::Cancel)
	{
		Bounds = Scenario.CancelChangesBounds;
	}
	else if (Decision == ESceneLifecycleDecision::Dismiss)
	{
		Bounds = Scenario.DiscardTitleBounds;
		Bounds.X = Bounds.Z - (Bounds.W - Bounds.Y);
	}
	else if (Decision == ESceneLifecycleDecision::Discard)
	{
		Bounds = Scenario.DiscardChangesBounds;
	}
	else
	{
		Bounds = Scenario.Bounds.Require(EEditorWidget::SaveSceneChanges);
	}
	if (ExerciseClick(InEvents, Bounds, Context.Click))
	{
		Context.Progress.TransitionTo(Decision == ESceneLifecycleDecision::SaveAs ||
		                                      Decision == ESceneLifecycleDecision::CancelSavePath
		                                  ? ESceneLifecycleState::AwaitSavePath
		                                  : ESceneLifecycleState::AwaitOutcome);
	}
}

void FEditorAcceptanceHarness::ExerciseSceneLifecycleSavePath(std::vector<FInputEvent>& InEvents)
{
	auto& Context = Scenario.SceneLifecycle;
	switch (Context.Progress.GetState())
	{
		case ESceneLifecycleState::AwaitSavePath:
			if (Editor.bSaveDialog && Scenario.Bounds.Contains(EEditorWidget::SavePath))
			{
				Context.Progress.TransitionTo(!Context.bSavingEmpty && Cases[Context.CaseIndex].Decision ==
				                                                           ESceneLifecycleDecision::CancelSavePath
				                                  ? ESceneLifecycleState::CancelSavePath
				                                  : ESceneLifecycleState::FocusSavePath);
			}
			break;
		case ESceneLifecycleState::FocusSavePath:
			if (ExerciseClick(InEvents, Scenario.Bounds.Require(EEditorWidget::SavePath), Context.Click))
			{
				Context.Progress.TransitionTo(ESceneLifecycleState::TypeSavePath);
			}
			break;
		case ESceneLifecycleState::TypeSavePath:
			if (ExerciseTextInput(InEvents, Context.PathInput, PathToUtf8(Context.Destination)))
			{
				Context.Progress.TransitionTo(ESceneLifecycleState::SubmitSavePath);
			}
			break;
		case ESceneLifecycleState::SubmitSavePath:
			if (ExerciseClick(InEvents, Scenario.Bounds.Require(EEditorWidget::SaveConfirm), Context.Click))
			{
				Context.Progress.TransitionTo(Context.bSavingEmpty ? ESceneLifecycleState::AwaitEmptySave
				                                                   : ESceneLifecycleState::AwaitOutcome);
			}
			break;
		case ESceneLifecycleState::CancelSavePath:
			if (ExerciseClick(InEvents, Scenario.Bounds.Require(EEditorWidget::SaveCancel), Context.Click))
			{
				Context.Progress.TransitionTo(ESceneLifecycleState::AwaitOutcome);
			}
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::VerifySceneLifecycleCase()
{
	auto& Context = Scenario.SceneLifecycle;
	if (Editor.PendingSceneChange || Editor.PendingSave || Editor.bSaveDialog || Editor.Transition.IsDecisionVisible())
	{
		return;
	}
	if (Editor.AssetWindow && Editor.WindowGroup->IsInputBlocked(Editor.AssetWindow->NativeWindow()))
	{
		return;
	}
	const auto Test = Cases[Context.CaseIndex];
	const auto Asset = Editor.AssetWorkspace->FindDocument(Context.AssetDocument);
	Check(Editor.AssetWindow && Asset && Asset->Document && Asset->Document->Generation() == Context.AssetGeneration &&
	          !Editor.WindowGroup->IsInputBlocked(Editor.AssetWindow->NativeWindow()),
	      "Scene decision changed the asset tab or retained its input block");
	const bool bCancel = Test.Decision == ESceneLifecycleDecision::Cancel ||
	                     Test.Decision == ESceneLifecycleDecision::Dismiss ||
	                     Test.Decision == ESceneLifecycleDecision::CancelSavePath;
	if (bCancel)
	{
		Check(Editor.SceneDocument.Id() == Context.OriginalDocument && Editor.IsDirty() &&
		          Editor.Scene->FindNode(Context.OriginalHandle) && !Editor.History.empty(),
		      "GUI cancellation lost the scene or history");
	}
	else
	{
		Check(Editor.SceneDocument.Id() != Context.OriginalDocument && !Editor.IsDirty() && Editor.History.empty() &&
		          !Editor.Selection && Editor.CurrentPath.empty(),
		      "GUI new/close retained old document state");
		Check(!Editor.Scene->FindNode(Context.OriginalHandle), "GUI new/close retained an old object handle");
		Check(Test.Action == ESceneDocumentAction::Close
		          ? Editor.Scene->GetStatus().bClosed
		          : Editor.Scene->GetStatus().bReady && Editor.Scene->GetNodes().empty(),
		      "GUI lifecycle produced the wrong scene state");
	}
	if (Test.Decision == ESceneLifecycleDecision::SaveAs || Test.Decision == ESceneLifecycleDecision::SaveNamed)
	{
		CheckSavedScene(*Editor.IO.FileSystem(), Context.Destination, 1);
	}
	else
	{
		Check(!std::filesystem::exists(Context.Destination), "Discard/cancel unexpectedly wrote a scene");
	}
	++Context.CaseIndex;
	Context.Progress.TransitionTo(Context.CaseIndex == Cases.size() ? ESceneLifecycleState::PrepareLateSave
	                                                                : ESceneLifecycleState::PrepareCase);
}

void FEditorAcceptanceHarness::CheckSceneLifecycleLateSave()
{
	auto& Context = Scenario.SceneLifecycle;
	if (Context.Fixture && Context.Fixture->Failure && !Context.Progress.Is(ESceneLifecycleState::AwaitCancelledSave))
	{
		std::rethrow_exception(Context.Fixture->Failure);
	}
	switch (Context.Progress.GetState())
	{
		case ESceneLifecycleState::PrepareLateSave:
		{
			const auto Current = Editor.DocumentStatus().Scene;
			Context.Fixture = Editor.ChangeDocument(ESceneDocumentAction::New, {Current.Document, Current.Revision});
			Context.Progress.TransitionTo(ESceneLifecycleState::AwaitLateSaveFixture);
			break;
		}
		case ESceneLifecycleState::AwaitLateSaveFixture:
			if (Context.Fixture->Result)
			{
				const auto Node =
				    CreateSceneNode(Editor.SceneDocument, {Editor.SceneDocument.Id(), Editor.Scene->GetRevision(),
				                                           "Retain after cancelled save"});
				Context.OriginalDocument = Node.Document;
				Context.OriginalHandle = Node.Handle;
				Context.Destination = Editor.Options.ExerciseSceneLifecycle / "CancelledSave.hasset";
				const auto Current = Editor.DocumentStatus().Scene;
				Context.Fixture = Editor.ChangeDocument(
				    ESceneDocumentAction::Close,
				    {Current.Document, Current.Revision, ESceneDirtyAction::Save, PathToUtf8(Context.Destination)});
				Check(Editor.PendingSave.has_value(), "Save-before-close did not admit IO");
				Editor.CancelDiscardAction();
				Check(Context.Fixture->Failure && !Editor.PendingSceneChange,
				      "Cancelling admitted save kept its scene intent");
				Context.Progress.TransitionTo(ESceneLifecycleState::AwaitCancelledSave);
			}
			break;
		case ESceneLifecycleState::AwaitCancelledSave:
			if (!Editor.PendingSave)
			{
				Check(Editor.SceneDocument.Id() == Context.OriginalDocument &&
				          Editor.Scene->FindNode(Context.OriginalHandle) && Editor.Scene->GetStatus().bReady &&
				          !Editor.IsDirty() && !Editor.History.empty(),
				      "Late save completion revived a cancelled close");
				CheckSavedScene(*Editor.IO.FileSystem(), Context.Destination, 1);
				const auto Current = Editor.DocumentStatus().Scene;
				Context.Fixture =
				    Editor.ChangeDocument(ESceneDocumentAction::Close, {Current.Document, Current.Revision});
				Context.Progress.TransitionTo(ESceneLifecycleState::AwaitFinalClose);
			}
			break;
		case ESceneLifecycleState::AwaitFinalClose:
			if (Context.Fixture->Result)
			{
				Check(Editor.Scene->GetStatus().bClosed, "Final GUI capture requires a closed scene");
				Context.Progress.TransitionTo(ESceneLifecycleState::Complete);
				Context.bVerified = true;
			}
			break;
		default:
			break;
	}
}

bool FEditorAcceptanceHarness::ExerciseSceneLifecycleStartup(std::vector<FInputEvent>& InEvents)
{
	auto& Context = Scenario.SceneLifecycle;
	switch (Context.Progress.GetState())
	{
		case ESceneLifecycleState::AwaitEmptyReady:
			if (Editor.Scene->GetStatus().bReady && !Context.ReadySettle.ConsumeFrame())
			{
				std::filesystem::create_directories(Editor.Options.ExerciseSceneLifecycle);
				Context.Destination = Editor.Options.ExerciseSceneLifecycle / "Empty.hasset";
				Editor.Gui->FocusWindow("Outliner");
				Context.Progress.TransitionTo(ESceneLifecycleState::PressSaveShortcut);
			}
			break;
		case ESceneLifecycleState::PressSaveShortcut:
		case ESceneLifecycleState::ReleaseSaveShortcut:
		{
			FInputEvent Event;
			Event.Type = EEventType::Key;
			Event.Key = EKey::S;
			Event.Modifiers = InputModifiers::Control;
			Event.bDown = Context.Progress.Is(ESceneLifecycleState::PressSaveShortcut);
			InEvents.push_back(Event);
			Context.Progress.TransitionTo(Event.bDown ? ESceneLifecycleState::ReleaseSaveShortcut
			                                          : ESceneLifecycleState::AwaitSavePath);
			break;
		}
		case ESceneLifecycleState::AwaitEmptySave:
			if (!Editor.PendingSave && !Editor.bSaveDialog)
			{
				CheckSavedScene(*Editor.IO.FileSystem(), Context.Destination, 0);
				Context.bSavingEmpty = false;
				Editor.RequestOpenAsset("/Engine/Models/Primitives/Cube.hasset");
				Context.Progress.TransitionTo(ESceneLifecycleState::AwaitAssetFixture);
			}
			break;
		case ESceneLifecycleState::AwaitAssetFixture:
			if (Editor.AssetWindow && Editor.AssetWorkspace->ActiveDocument())
			{
				Context.AssetDocument = Editor.AssetWorkspace->Documents().front().Id;
				Context.AssetGeneration = Editor.AssetWorkspace->ActiveDocument()->Generation();
				Context.Progress.TransitionTo(ESceneLifecycleState::PrepareCase);
			}
			break;
		default:
			return false;
	}
	return true;
}

void FEditorAcceptanceHarness::ExerciseSceneLifecycleInput(std::vector<FInputEvent>& InEvents)
{
	if (ExerciseSceneLifecycleStartup(InEvents))
	{
		return;
	}
	auto& Context = Scenario.SceneLifecycle;
	switch (Context.Progress.GetState())
	{
		case ESceneLifecycleState::PrepareCase:
		case ESceneLifecycleState::AwaitFixture:
		case ESceneLifecycleState::AwaitFixtureSave:
			PrepareSceneLifecycleCase();
			break;
		case ESceneLifecycleState::OpenFileMenu:
			if (ExerciseClick(InEvents, Scenario.FileMenuBounds, Context.Click))
			{
				Context.Progress.TransitionTo(ESceneLifecycleState::InvokeCommand);
			}
			break;
		case ESceneLifecycleState::InvokeCommand:
			if (ExerciseClick(InEvents,
			                  Scenario.Bounds.Require(Cases[Context.CaseIndex].Action == ESceneDocumentAction::New
			                                              ? EEditorWidget::NewScene
			                                              : EEditorWidget::CloseScene),
			                  Context.Click))
			{
				Context.Progress.TransitionTo(ESceneLifecycleState::AwaitDecision);
			}
			break;
		case ESceneLifecycleState::AwaitDecision:
			if (Editor.Transition.IsDecisionVisible() && Editor.Transition.HasPendingScene())
			{
				Check(Editor.SceneDocument.Id() == Context.OriginalDocument && Editor.IsDirty(),
				      "Scene retired before the GUI decision");
				Check(Editor.WindowGroup->IsInputBlocked(Editor.AssetWindow->NativeWindow()) &&
				          Editor.AssetWorkspace->IsBlocked(),
				      "Scene decision did not block the auxiliary asset window");
				Context.Progress.TransitionTo(ESceneLifecycleState::ChooseDecision);
			}
			break;
		case ESceneLifecycleState::ChooseDecision:
			ExerciseSceneLifecycleDecision(InEvents);
			break;
		case ESceneLifecycleState::AwaitOutcome:
			VerifySceneLifecycleCase();
			break;
		case ESceneLifecycleState::Complete:
			break;
		case ESceneLifecycleState::PrepareLateSave:
		case ESceneLifecycleState::AwaitLateSaveFixture:
		case ESceneLifecycleState::AwaitCancelledSave:
		case ESceneLifecycleState::AwaitFinalClose:
			CheckSceneLifecycleLateSave();
			break;
		default:
			ExerciseSceneLifecycleSavePath(InEvents);
			break;
	}
}
} // namespace Hyperion
