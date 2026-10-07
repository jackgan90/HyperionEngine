#include "EditorDocumentTransition.h"

namespace Hyperion
{
void FEditorDocumentTransition::QueueScene(ESceneDocumentAction InAction, FSceneLifecycleRequest InRequest,
                                           bool bInDecision)
{
	if (HasPendingScene() || HasPendingRoot() || HasPendingClose() || IsDecisionVisible())
	{
		throw std::logic_error("Another document transition is active");
	}
	Scene = FEditorSceneChange{InAction, std::move(InRequest),
	                           bInDecision ? EEditorTransitionPhase::AwaitingDecision : EEditorTransitionPhase::Ready};
	if (bInDecision)
	{
		ShowDecision();
	}
}

bool FEditorDocumentTransition::HasPendingScene() const
{
	return Scene.has_value();
}

bool FEditorDocumentTransition::IsSavingScene() const
{
	return ScenePhase() == EEditorTransitionPhase::Saving || ScenePhase() == EEditorTransitionPhase::AwaitingSavePath;
}

EEditorTransitionPhase FEditorDocumentTransition::ScenePhase() const
{
	return Scene ? Scene->Phase : EEditorTransitionPhase::Idle;
}

const FEditorSceneChange& FEditorDocumentTransition::SceneChange() const
{
	if (!Scene)
	{
		throw std::logic_error("No scene transition is active");
	}
	return *Scene;
}

std::optional<FEditorSceneChange> FEditorDocumentTransition::TakeSceneCommit(const FEditorSaveProgress& InProgress)
{
	if (!Scene || InProgress.bSceneSaving || InProgress.bSaveDialog)
	{
		return {};
	}
	if (Scene->Phase == EEditorTransitionPhase::Saving)
	{
		Scene->Phase = InProgress.bSceneDirty ? EEditorTransitionPhase::Failed : EEditorTransitionPhase::Ready;
	}
	if (Scene->Phase != EEditorTransitionPhase::Ready)
	{
		return {};
	}
	Scene->Phase = EEditorTransitionPhase::Preparing;
	return Scene;
}

void FEditorDocumentTransition::FinishSceneChange()
{
	Scene.reset();
	if (!HasPendingRoot() && !HasPendingClose() && PendingOpen.empty())
	{
		Decision = EDecisionPresentation::Hidden;
	}
}
} // namespace Hyperion
