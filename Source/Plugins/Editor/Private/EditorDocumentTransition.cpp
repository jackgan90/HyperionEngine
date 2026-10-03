#include "EditorDocumentTransition.h"
#include <utility>

namespace Hyperion
{
void FEditorDocumentTransition::Cancel()
{
	Root.reset();
	Close.reset();
	PendingOpen.clear();
	Decision = EDecisionPresentation::Hidden;
}

void FEditorDocumentTransition::QueueOpen(const std::string& InPath)
{
	PendingOpen = InPath;
	ShowDecision();
}

void FEditorDocumentTransition::QueueRoot(const std::filesystem::path& InPath)
{
	if (!HasPendingRoot() && !InPath.empty())
	{
		Root.emplace();
		Root->Requested = InPath;
	}
}

bool FEditorDocumentTransition::RequestWindowClose(const FEditorSaveProgress& InProgress)
{
	if (!InProgress.bSceneDirty && !InProgress.bAssetsDirty && !InProgress.bImportDirty && !InProgress.bSceneSaving &&
	    !InProgress.bAssetsSaving)
	{
		CompleteClose();
		return true;
	}
	if (!Close)
	{
		Close.emplace();
	}
	else if (Close->Phase == EEditorTransitionPhase::Ready)
	{
		Close->Phase = EEditorTransitionPhase::Reconfirming;
	}
	ShowDecision();
	return false;
}

std::optional<std::filesystem::path> FEditorDocumentTransition::TakeRootRequest()
{
	if (RootPhase() != EEditorTransitionPhase::Requested)
	{
		return {};
	}
	Root->Phase = EEditorTransitionPhase::Preparing;
	return std::exchange(Root->Requested, {});
}

void FEditorDocumentTransition::AcceptRootCandidate(FContentRootCandidate InCandidate,
                                                    const FEditorSaveProgress& InProgress)
{
	if (RootPhase() != EEditorTransitionPhase::Preparing)
	{
		throw std::logic_error("A root candidate requires an active preparation request");
	}
	Root->Candidate.emplace(std::move(InCandidate));
	const bool bNeedsDecision = InProgress.bSceneDirty || InProgress.bAssetsDirty || InProgress.bImportDirty ||
	                            InProgress.bSceneSaving || InProgress.bAssetsSaving;
	Root->Phase = bNeedsDecision ? EEditorTransitionPhase::AwaitingDecision : EEditorTransitionPhase::Ready;
	if (bNeedsDecision)
	{
		ShowDecision();
	}
}

void FEditorDocumentTransition::FinishRootRequest()
{
	Root.reset();
	if (!HasPendingClose() && PendingOpen.empty())
	{
		Decision = EDecisionPresentation::Hidden;
	}
}

void FEditorDocumentTransition::ContentRootChanged()
{
	Root.reset();
	PendingOpen.clear();
	Decision = EDecisionPresentation::Hidden;
}

void FEditorDocumentTransition::ContentDocumentClosed()
{
	PendingOpen.clear();
}

void FEditorDocumentTransition::BeginSave(EEditorTransitionTarget InTarget)
{
	if (InTarget == EEditorTransitionTarget::Close)
	{
		Close.emplace();
		Close->Phase = EEditorTransitionPhase::Saving;
	}
	else if (InTarget == EEditorTransitionTarget::Root)
	{
		RequirePreparedRoot().Phase = EEditorTransitionPhase::Saving;
	}
	else
	{
		throw std::logic_error("Only root and close decisions support save continuations");
	}
}

void FEditorDocumentTransition::AwaitSavePath(EEditorTransitionTarget InTarget)
{
	if (InTarget == EEditorTransitionTarget::Close)
	{
		Close.emplace();
		Close->Phase = EEditorTransitionPhase::AwaitingSavePath;
	}
	else if (InTarget == EEditorTransitionTarget::Root)
	{
		RequirePreparedRoot().Phase = EEditorTransitionPhase::AwaitingSavePath;
	}
	else
	{
		throw std::logic_error("Only root and close decisions can await a save path");
	}
	Decision = EDecisionPresentation::Hidden;
}

void FEditorDocumentTransition::SaveAdmitted(EEditorTransitionTarget InTarget, bool bInSceneSaving)
{
	if (InTarget == EEditorTransitionTarget::Close && IsSavingClose())
	{
		Close->Phase = EEditorTransitionPhase::Saving;
		// Choosing save-and-exit supersedes replacement intents, even if the save later fails.
		Root.reset();
		PendingOpen.clear();
		Decision = EDecisionPresentation::Hidden;
	}
	else if (InTarget == EEditorTransitionTarget::Root && IsSavingRoot())
	{
		RequirePreparedRoot().Phase = EEditorTransitionPhase::Saving;
		if (!bInSceneSaving)
		{
			Decision = EDecisionPresentation::Hidden;
		}
	}
	else
	{
		throw std::logic_error("A save admission requires an active continuation");
	}
}

void FEditorDocumentTransition::SaveRejected(EEditorTransitionTarget InTarget, const std::string& InError)
{
	if (InTarget == EEditorTransitionTarget::Close && Close)
	{
		Close->Phase = EEditorTransitionPhase::Failed;
		Close->Error = InError;
	}
	else if (InTarget == EEditorTransitionTarget::Root && HasPendingRoot())
	{
		Root->Phase = EEditorTransitionPhase::AwaitingDecision;
		ShowDecision();
	}
}

void FEditorDocumentTransition::CancelSaveDialog()
{
	if (IsSavingRoot() || HasPendingClose())
	{
		Cancel();
	}
}

bool FEditorDocumentTransition::SceneSaveFailed(const std::string& InError)
{
	if (IsSavingClose())
	{
		Close->Error = InError;
	}
	const bool bHadRoot = Root.has_value();
	if (bHadRoot)
	{
		Root.reset();
		Decision = EDecisionPresentation::Hidden;
	}
	return bHadRoot;
}

bool FEditorDocumentTransition::AdvanceRootSave(const FEditorSaveProgress& InProgress)
{
	if (RootPhase() != EEditorTransitionPhase::Saving || InProgress.bSaveDialog || InProgress.bSceneSaving ||
	    InProgress.bAssetsSaving)
	{
		return false;
	}
	if (InProgress.bSceneDirty || InProgress.bAssetsDirty)
	{
		Root->Phase = EEditorTransitionPhase::AwaitingDecision;
		ShowDecision();
		return false;
	}
	Root->Phase = EEditorTransitionPhase::Ready;
	Decision = EDecisionPresentation::Hidden;
	return true;
}

bool FEditorDocumentTransition::AdvanceCloseSave(const FEditorSaveProgress& InProgress)
{
	if (ClosePhase() != EEditorTransitionPhase::Saving || InProgress.bSaveDialog || InProgress.bSceneSaving ||
	    InProgress.bAssetsSaving || InProgress.bPendingAssetEdits)
	{
		return false;
	}
	if (!InProgress.bSceneDirty && !InProgress.bAssetsDirty)
	{
		CompleteClose();
		return true;
	}
	Close->Phase = EEditorTransitionPhase::Failed;
	if (Close->Error.empty())
	{
		Close->Error = "Documents remain dirty after save; inspect asset.info or scene.status before retrying";
	}
	ShowDecision();
	return false;
}

std::optional<FEditorRootCommit> FEditorDocumentTransition::TakeRootCommit(const FEditorSaveProgress& InProgress)
{
	if (RootPhase() != EEditorTransitionPhase::Ready || InProgress.bSceneSaving || InProgress.bAssetsSaving ||
	    InProgress.bPendingAssetEdits)
	{
		return {};
	}
	FEditorRootCommit Result{Root->Candidate->GetDirectory(), Root->bDiscard};
	Root.reset();
	return Result;
}

FEditorDiscardRequest FEditorDocumentTransition::ConfirmDiscard()
{
	if (!IsDecisionVisible())
	{
		throw std::logic_error("Discard requires a pending document decision");
	}
	FEditorDiscardRequest Result{DiscardTarget()};
	Decision = EDecisionPresentation::Hidden;
	if (Result.Target == EEditorTransitionTarget::Root)
	{
		Root->bDiscard = true;
		Root->Phase = EEditorTransitionPhase::Ready;
	}
	else if (Result.Target == EEditorTransitionTarget::Open)
	{
		Result.OpenPath = std::exchange(PendingOpen, {});
	}
	return Result;
}

void FEditorDocumentTransition::CompleteClose()
{
	Root.reset();
	PendingOpen.clear();
	Close.emplace();
	Close->Phase = EEditorTransitionPhase::Ready;
	Decision = EDecisionPresentation::Hidden;
}

bool FEditorDocumentTransition::TakeDecisionRequest()
{
	if (Decision != EDecisionPresentation::Requested)
	{
		return false;
	}
	Decision = EDecisionPresentation::Visible;
	return true;
}

void FEditorDocumentTransition::ShowDecision()
{
	Decision = EDecisionPresentation::Requested;
}

bool FEditorDocumentTransition::IsDecisionVisible() const
{
	return Decision != EDecisionPresentation::Hidden;
}

bool FEditorDocumentTransition::HasPendingRoot() const
{
	return Root && Root->Candidate.has_value();
}

bool FEditorDocumentTransition::HasCloseRequest() const
{
	return Close.has_value();
}

bool FEditorDocumentTransition::HasPendingClose() const
{
	return Close && Close->Phase != EEditorTransitionPhase::Ready;
}

bool FEditorDocumentTransition::IsSavingRoot() const
{
	return RootPhase() == EEditorTransitionPhase::Saving || RootPhase() == EEditorTransitionPhase::AwaitingSavePath;
}

bool FEditorDocumentTransition::IsSavingClose() const
{
	return ClosePhase() == EEditorTransitionPhase::Saving || ClosePhase() == EEditorTransitionPhase::AwaitingSavePath;
}

EEditorTransitionPhase FEditorDocumentTransition::RootPhase() const
{
	return Root ? Root->Phase : EEditorTransitionPhase::Idle;
}

EEditorTransitionPhase FEditorDocumentTransition::ClosePhase() const
{
	return Close ? Close->Phase : EEditorTransitionPhase::Idle;
}

std::string_view FEditorDocumentTransition::CloseStatus() const
{
	switch (ClosePhase())
	{
		case EEditorTransitionPhase::Saving:
			return "saving";
		case EEditorTransitionPhase::Failed:
			return "failed";
		case EEditorTransitionPhase::Ready:
		case EEditorTransitionPhase::Reconfirming:
			return "closing";
		default:
			return "idle";
	}
}

const std::string& FEditorDocumentTransition::CloseError() const
{
	static const std::string Empty;
	return Close ? Close->Error : Empty;
}

EEditorTransitionTarget FEditorDocumentTransition::DiscardTarget() const
{
	if (HasPendingClose())
	{
		return EEditorTransitionTarget::Close;
	}
	if (HasPendingRoot())
	{
		return EEditorTransitionTarget::Root;
	}
	return PendingOpen.empty() ? EEditorTransitionTarget::Document : EEditorTransitionTarget::Open;
}

FEditorDocumentTransition::FRootRequest& FEditorDocumentTransition::RequirePreparedRoot()
{
	if (!HasPendingRoot())
	{
		throw std::logic_error("A root save requires a prepared root transition");
	}
	return *Root;
}
} // namespace Hyperion
