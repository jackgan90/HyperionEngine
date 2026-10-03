#include "EditorDocumentTransition.h"
#include <functional>
#include <source_location>

using namespace Hyperion;

namespace
{
void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Editor transition check failed at " + std::to_string(InLocation.line()));
	}
}

void Rejects(const std::function<void()>& InAction)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::logic_error&)
	{
		bRejected = true;
	}
	Check(bRejected);
}

void PrepareRoot(FEditorDocumentTransition& InTransition, bool bInNeedsDecision = true)
{
	InTransition.QueueRoot("Root-B");
	Check(InTransition.TakeRootRequest() == std::filesystem::path("Root-B"));
	Check(!InTransition.TakeRootRequest());
	// The owner treats the candidate as opaque. Real-service tests verify its directory and admission.
	InTransition.AcceptRootCandidate({}, {.bSceneDirty = bInNeedsDecision});
}

void CheckOpenAndCancel()
{
	FEditorDocumentTransition Transition;
	Check(Transition.CloseStatus() == "idle" && Transition.CloseError().empty());
	Check(!Transition.TakeDecisionRequest() && !Transition.TakeRootRequest());
	Rejects(
	    [&]
	    {
		    Transition.ConfirmDiscard();
	    });
	Transition.QueueOpen("/Game/Next.hasset");
	Check(Transition.IsDecisionVisible() && Transition.TakeDecisionRequest());
	Check(!Transition.TakeDecisionRequest() && Transition.IsDecisionVisible());
	const auto Open = Transition.ConfirmDiscard();
	Check(Open.Target == EEditorTransitionTarget::Open && Open.OpenPath == "/Game/Next.hasset");
	Check(!Transition.IsDecisionVisible());
	Rejects(
	    [&]
	    {
		    Transition.ConfirmDiscard();
	    });
	Transition.QueueOpen("/Game/Cancelled.hasset");
	Transition.Cancel();
	Check(!Transition.IsDecisionVisible() && !Transition.TakeDecisionRequest());
	Transition.QueueRoot("Queued-but-cancelled");
	Transition.Cancel();
	Check(!Transition.TakeRootRequest());
	Check(!Transition.AdvanceRootSave({}) && !Transition.AdvanceCloseSave({}));
	Check(!Transition.TakeRootCommit({}));
}

void CheckDirtyAndBusyAdmission()
{
	for (const auto& Progress : {FEditorSaveProgress{.bSceneDirty = true}, FEditorSaveProgress{.bAssetsDirty = true},
	                             FEditorSaveProgress{.bImportDirty = true}, FEditorSaveProgress{.bSceneSaving = true},
	                             FEditorSaveProgress{.bAssetsSaving = true}})
	{
		FEditorDocumentTransition Transition;
		Check(!Transition.RequestWindowClose(Progress));
		Check(Transition.HasPendingClose() && Transition.IsDecisionVisible());
		Transition.Cancel();
		Transition.QueueRoot("Root-B");
		Check(Transition.TakeRootRequest().has_value());
		Transition.AcceptRootCandidate({}, Progress);
		Check(Transition.IsDecisionVisible() && !Transition.TakeRootCommit({}));
	}
	FEditorDocumentTransition Clean;
	Check(Clean.RequestWindowClose({}) && Clean.CloseStatus() == "closing");
	Check(!Clean.RequestWindowClose({.bSceneDirty = true}));
	Check(Clean.HasPendingClose() && Clean.IsDecisionVisible() && Clean.CloseStatus() == "closing");
	Check(Clean.ConfirmDiscard().Target == EEditorTransitionTarget::Close);
	Clean.Cancel();
	Check(!Clean.HasCloseRequest() && Clean.CloseStatus() == "idle");
}

void CheckRootAdmissionAndDiscard()
{
	FEditorDocumentTransition Transition;
	Rejects(
	    [&]
	    {
		    Transition.BeginSave(EEditorTransitionTarget::Root);
	    });
	Rejects(
	    [&]
	    {
		    Transition.AcceptRootCandidate({}, {.bSceneDirty = true});
	    });
	Transition.QueueRoot({});
	Check(!Transition.TakeRootRequest());
	PrepareRoot(Transition);
	Check(Transition.HasPendingRoot() && Transition.RootPhase() == EEditorTransitionPhase::AwaitingDecision);
	Transition.QueueRoot("Ignored-while-prepared");
	Check(!Transition.TakeRootRequest());
	Check(!Transition.TakeRootCommit({}));
	const auto Discard = Transition.ConfirmDiscard();
	Check(Discard.Target == EEditorTransitionTarget::Root && !Transition.IsDecisionVisible());
	Check(!Transition.TakeRootCommit({.bSceneSaving = true}));
	Check(!Transition.TakeRootCommit({.bAssetsSaving = true}));
	Check(!Transition.TakeRootCommit({.bPendingAssetEdits = true}));
	const auto Commit = Transition.TakeRootCommit({.bSceneDirty = true, .bAssetsDirty = true});
	Check(Commit && Commit->bDiscard && !Transition.HasPendingRoot());
	Check(!Transition.TakeRootCommit({}));
	PrepareRoot(Transition, false);
	const auto Clean = Transition.TakeRootCommit({});
	Check(Clean && !Clean->bDiscard);
	Transition.QueueRoot("Invalid-root");
	Check(Transition.TakeRootRequest().has_value());
	Transition.FinishRootRequest();
	Check(Transition.RootPhase() == EEditorTransitionPhase::Idle && !Transition.IsDecisionVisible());
	PrepareRoot(Transition);
	Transition.ContentRootChanged();
	Check(!Transition.HasPendingRoot() && !Transition.TakeDecisionRequest());
}

void CheckRootSaves()
{
	FEditorDocumentTransition Transition;
	PrepareRoot(Transition);
	Transition.BeginSave(EEditorTransitionTarget::Root);
	Transition.SaveAdmitted(EEditorTransitionTarget::Root, true);
	Check(Transition.IsSavingRoot() && Transition.IsDecisionVisible());
	Check(!Transition.AdvanceRootSave({.bSceneDirty = true, .bSceneSaving = true}));
	Check(!Transition.AdvanceRootSave({.bAssetsSaving = true}));
	Check(!Transition.AdvanceRootSave({.bSaveDialog = true}));
	Check(!Transition.AdvanceRootSave({.bAssetsDirty = true}));
	Check(!Transition.IsSavingRoot() && Transition.TakeDecisionRequest());
	Check(!Transition.TakeRootCommit({}));
	Transition.BeginSave(EEditorTransitionTarget::Root);
	Transition.SaveRejected(EEditorTransitionTarget::Root, "save admission failure");
	Check(Transition.HasPendingRoot() && !Transition.IsSavingRoot() && Transition.IsDecisionVisible());
	Transition.BeginSave(EEditorTransitionTarget::Root);
	Transition.SaveAdmitted(EEditorTransitionTarget::Root, false);
	Check(!Transition.IsDecisionVisible());
	Check(Transition.AdvanceRootSave({}));
	Check(!Transition.AdvanceRootSave({}));
	const auto Commit = Transition.TakeRootCommit({});
	Check(Commit && !Commit->bDiscard);
	PrepareRoot(Transition);
	Transition.BeginSave(EEditorTransitionTarget::Root);
	Check(Transition.SceneSaveFailed("asynchronous scene save failure"));
	Check(!Transition.HasPendingRoot() && !Transition.IsDecisionVisible());
	Check(!Transition.AdvanceRootSave({}) && !Transition.TakeRootCommit({}));
}

void CheckCancellationDuringSaves()
{
	FEditorDocumentTransition Transition;
	PrepareRoot(Transition);
	Transition.BeginSave(EEditorTransitionTarget::Root);
	Transition.SaveAdmitted(EEditorTransitionTarget::Root, true);
	Check(!Transition.AdvanceRootSave({.bSceneSaving = true}));
	Transition.Cancel();
	Check(!Transition.AdvanceRootSave({}) && !Transition.TakeRootCommit({}));
	Check(!Transition.SceneSaveFailed("late failure") && !Transition.TakeDecisionRequest());
	Transition.BeginSave(EEditorTransitionTarget::Close);
	Transition.SaveAdmitted(EEditorTransitionTarget::Close, true);
	Check(Transition.CloseStatus() == "saving");
	Transition.Cancel();
	Check(!Transition.AdvanceCloseSave({}) && Transition.CloseStatus() == "idle");
	Check(!Transition.SceneSaveFailed("late failure") && Transition.CloseError().empty());
	for (const auto Target : {EEditorTransitionTarget::Root, EEditorTransitionTarget::Close})
	{
		if (Target == EEditorTransitionTarget::Root)
		{
			PrepareRoot(Transition);
		}
		Transition.AwaitSavePath(Target);
		Check(!Transition.IsDecisionVisible());
		Check(!Transition.AdvanceRootSave({}) && !Transition.AdvanceCloseSave({}));
		Transition.CancelSaveDialog();
		Check(!Transition.HasPendingRoot() && !Transition.HasCloseRequest());
		Check(!Transition.AdvanceRootSave({}) && !Transition.AdvanceCloseSave({}));
	}
}

void CheckCloseFailureAndCompletion()
{
	FEditorDocumentTransition Transition;
	Check(!Transition.RequestWindowClose({.bSceneDirty = true}));
	Check(Transition.HasPendingClose() && Transition.CloseStatus() == "idle");
	Transition.BeginSave(EEditorTransitionTarget::Close);
	Transition.SaveRejected(EEditorTransitionTarget::Close, "admission failure");
	Check(Transition.CloseStatus() == "failed" && Transition.CloseError() == "admission failure");
	Check(Transition.HasPendingClose() && !Transition.AdvanceCloseSave({}));
	Transition.BeginSave(EEditorTransitionTarget::Close);
	Transition.SaveAdmitted(EEditorTransitionTarget::Close, true);
	Check(Transition.CloseStatus() == "saving" && Transition.CloseError().empty());
	Check(!Transition.IsDecisionVisible());
	Check(!Transition.AdvanceCloseSave({.bSceneSaving = true}));
	Check(!Transition.AdvanceCloseSave({.bAssetsSaving = true}));
	Check(!Transition.AdvanceCloseSave({.bPendingAssetEdits = true}));
	Check(!Transition.AdvanceCloseSave({.bSaveDialog = true}));
	Check(!Transition.SceneSaveFailed("disk failure"));
	Check(!Transition.AdvanceCloseSave({.bSceneDirty = true}));
	Check(Transition.CloseStatus() == "failed" && Transition.CloseError() == "disk failure");
	Check(Transition.HasPendingClose() && Transition.IsDecisionVisible() && Transition.TakeDecisionRequest());
	Transition.Cancel();
	Check(Transition.CloseStatus() == "idle" && !Transition.HasCloseRequest());
	Transition.BeginSave(EEditorTransitionTarget::Close);
	Transition.SaveAdmitted(EEditorTransitionTarget::Close, false);
	Check(!Transition.AdvanceCloseSave({.bAssetsDirty = true}));
	Check(Transition.CloseStatus() == "failed" && !Transition.CloseError().empty());
	Transition.BeginSave(EEditorTransitionTarget::Close);
	Transition.SaveAdmitted(EEditorTransitionTarget::Close, false);
	Check(Transition.AdvanceCloseSave({}) && Transition.CloseStatus() == "closing");
	Check(!Transition.HasPendingClose() && !Transition.IsDecisionVisible());
	Check(!Transition.AdvanceCloseSave({}));
}

void CheckOverlappingTargets()
{
	FEditorDocumentTransition Transition;
	Transition.QueueOpen("/Game/Next.hasset");
	PrepareRoot(Transition);
	Check(!Transition.RequestWindowClose({.bSceneDirty = true}));
	Check(Transition.HasPendingRoot() && Transition.HasPendingClose());
	Check(Transition.ConfirmDiscard().Target == EEditorTransitionTarget::Close);
	Transition.CompleteClose();
	Check(Transition.CloseStatus() == "closing" && !Transition.HasPendingRoot());
	Check(!Transition.TakeRootCommit({}) && !Transition.TakeDecisionRequest());
	Transition.Cancel();
	Transition.QueueOpen("/Game/Next.hasset");
	PrepareRoot(Transition);
	Check(Transition.ConfirmDiscard().Target == EEditorTransitionTarget::Root);
	Check(Transition.TakeRootCommit({}).has_value());
	Transition.ContentDocumentClosed();
	Transition.ContentRootChanged();
	Check(!Transition.IsDecisionVisible());
	PrepareRoot(Transition);
	Check(!Transition.RequestWindowClose({.bSceneDirty = true}));
	Transition.BeginSave(EEditorTransitionTarget::Close);
	Transition.SaveAdmitted(EEditorTransitionTarget::Close, true);
	Check(!Transition.HasPendingRoot());
	Check(!Transition.SceneSaveFailed("close save failure"));
	Check(!Transition.AdvanceCloseSave({.bSceneDirty = true}));
	Check(Transition.HasPendingClose() && Transition.CloseStatus() == "failed");
	Check(Transition.ConfirmDiscard().Target == EEditorTransitionTarget::Close);
	Transition.Cancel();
	PrepareRoot(Transition);
	Check(!Transition.RequestWindowClose({.bSceneDirty = true}));
	Transition.BeginSave(EEditorTransitionTarget::Close);
	Transition.SaveAdmitted(EEditorTransitionTarget::Close, true);
	Transition.Cancel();
	Check(!Transition.AdvanceCloseSave({}) && !Transition.TakeRootCommit({}));
	Check(!Transition.HasPendingClose() && !Transition.HasPendingRoot());
}
} // namespace

int main()
{
	CheckOpenAndCancel();
	CheckDirtyAndBusyAdmission();
	CheckRootAdmissionAndDiscard();
	CheckRootSaves();
	CheckCancellationDuringSaves();
	CheckCloseFailureAndCompletion();
	CheckOverlappingTargets();
}
