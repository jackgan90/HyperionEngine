#include "EditorDocumentTransition.h"

namespace Hyperion
{
void FEditorDocumentTransition::Cancel()
{
	CloseState = "idle";
	CloseError.clear();
	bDiscardDialog = bPendingClose = bSaveThenClose = false;
	PendingOpen.clear();
	PendingRoot.reset();
	bCommitRoot = bSaveThenSwitch = false;
}

void FEditorDocumentTransition::QueueOpen(const std::string& InPath)
{
	PendingOpen = InPath;
	bDiscardDialog = bRequestDiscard = true;
}

void FEditorDocumentTransition::QueueRoot(const std::filesystem::path& InPath)
{
	if (!PendingRoot)
	{
		RequestedRoot = InPath;
	}
}

EEditorDiscardAction FEditorDocumentTransition::DiscardAction() const
{
	if (bPendingClose)
	{
		return EEditorDiscardAction::Close;
	}
	if (PendingRoot)
	{
		return EEditorDiscardAction::Root;
	}
	return PendingOpen.empty() ? EEditorDiscardAction::Document : EEditorDiscardAction::Open;
}

void FEditorDocumentTransition::ConfirmRootDiscard()
{
	bDiscardRoot = true;
	bCommitRoot = true;
}

void FEditorDocumentTransition::FinishDiscard()
{
	bPendingClose = bSaveThenClose = false;
}
} // namespace Hyperion
