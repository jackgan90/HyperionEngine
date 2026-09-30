#pragma once
#include "Hyperion/Content/ContentRootService.h"

namespace Hyperion
{
enum class EEditorDiscardAction
{
	Document,
	Close,
	Root,
	Open
};

// Pending decisions are independent of GUI rendering, provider lookup and document implementation.
class FEditorDocumentTransition
{
public:
	void Cancel();
	void QueueOpen(const std::string& InPath);
	void QueueRoot(const std::filesystem::path& InPath);
	EEditorDiscardAction DiscardAction() const;
	void ConfirmRootDiscard();
	void FinishDiscard();
	std::string CloseState = "idle";
	std::string CloseError;
	std::filesystem::path RequestedRoot;
	std::optional<FContentRootCandidate> PendingRoot;
	bool bDiscardRoot{};
	bool bSaveThenSwitch{};
	bool bCommitRoot{};
	bool bDiscardDialog{};
	bool bRequestDiscard{};
	bool bPendingClose{};
	bool bSaveThenClose{};
	std::string PendingOpen;
};
} // namespace Hyperion
