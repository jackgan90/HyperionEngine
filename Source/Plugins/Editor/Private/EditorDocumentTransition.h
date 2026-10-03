#pragma once
#include "Hyperion/Content/ContentRootService.h"

namespace Hyperion
{
enum class EEditorTransitionTarget
{
	Document,
	Close,
	Root,
	Open
};

enum class EEditorTransitionPhase
{
	Idle,
	Requested,
	Preparing,
	AwaitingDecision,
	Reconfirming,
	AwaitingSavePath,
	Saving,
	Ready,
	Failed
};

struct FEditorSaveProgress
{
	bool bSceneDirty{};
	bool bAssetsDirty{};
	bool bImportDirty{};
	bool bSceneSaving{};
	bool bAssetsSaving{};
	bool bPendingAssetEdits{};
	bool bSaveDialog{};
};

struct FEditorDiscardRequest
{
	EEditorTransitionTarget Target = EEditorTransitionTarget::Document;
	std::string OpenPath;
};

struct FEditorRootCommit
{
	std::filesystem::path Directory;
	bool bDiscard{};
};

// Main-only decisions; hosts supply facts and execute effects without writing transition state.
class FEditorDocumentTransition
{
public:
	void Cancel();
	void QueueOpen(const std::string& InPath);
	void QueueRoot(const std::filesystem::path& InPath);
	bool RequestWindowClose(const FEditorSaveProgress& InProgress);
	std::optional<std::filesystem::path> TakeRootRequest();
	void AcceptRootCandidate(FContentRootCandidate InCandidate, const FEditorSaveProgress& InProgress);
	void FinishRootRequest();
	void ContentRootChanged();
	void ContentDocumentClosed();
	void BeginSave(EEditorTransitionTarget InTarget);
	void AwaitSavePath(EEditorTransitionTarget InTarget);
	void SaveAdmitted(EEditorTransitionTarget InTarget, bool bInSceneSaving);
	void SaveRejected(EEditorTransitionTarget InTarget, const std::string& InError);
	void CancelSaveDialog();
	bool SceneSaveFailed(const std::string& InError);
	bool AdvanceRootSave(const FEditorSaveProgress& InProgress);
	bool AdvanceCloseSave(const FEditorSaveProgress& InProgress);
	std::optional<FEditorRootCommit> TakeRootCommit(const FEditorSaveProgress& InProgress);
	FEditorDiscardRequest ConfirmDiscard();
	void CompleteClose();
	bool TakeDecisionRequest();
	bool IsDecisionVisible() const;
	bool HasPendingRoot() const;
	bool HasCloseRequest() const;
	bool HasPendingClose() const;
	bool IsSavingRoot() const;
	bool IsSavingClose() const;
	EEditorTransitionPhase RootPhase() const;
	EEditorTransitionPhase ClosePhase() const;
	std::string_view CloseStatus() const;
	const std::string& CloseError() const;

private:
	enum class EDecisionPresentation
	{
		Hidden,
		Requested,
		Visible
	};

	struct FRootRequest
	{
		std::filesystem::path Requested;
		std::optional<FContentRootCandidate> Candidate;
		EEditorTransitionPhase Phase = EEditorTransitionPhase::Requested;
		bool bDiscard{};
	};

	struct FCloseRequest
	{
		EEditorTransitionPhase Phase = EEditorTransitionPhase::AwaitingDecision;
		std::string Error;
	};

	void ShowDecision();
	EEditorTransitionTarget DiscardTarget() const;
	FRootRequest& RequirePreparedRoot();
	std::optional<FRootRequest> Root;
	std::optional<FCloseRequest> Close;
	std::string PendingOpen;
	EDecisionPresentation Decision = EDecisionPresentation::Hidden;
};
} // namespace Hyperion
