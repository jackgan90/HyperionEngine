#pragma once
#include "Hyperion/SceneEditing/SceneClipboard.h"
#include "Hyperion/SceneEditing/SceneEditTarget.h"
#include "Hyperion/SceneEditing/SceneHistory.h"

namespace Hyperion
{
class FSceneEditError : public std::runtime_error
{
public:
	FSceneEditError(std::string InCode, std::string InMessage);
	std::string Code;
};

struct FSceneInspectorTransaction
{
	std::uint64_t Interaction{};
	FSceneHandle Handle;
	std::size_t HistoryIndex{};
	std::uint64_t Revision{};
	std::vector<FSceneHandle> Targets;
};

struct FScenePendingSave
{
	TAsyncResult<bool> Result;
	std::string Destination;
	std::uint64_t Epoch{};
	std::uint64_t State{};
	std::uint64_t Started{};
};

struct FSceneDocumentState
{
	std::vector<FSceneHistoryEntry> History;
	std::size_t HistoryCursor{};
	std::uint64_t NextState{};
	std::uint64_t State{};
	std::uint64_t SavedState{};
	std::uint64_t Epoch{};
	std::string Path;
	std::optional<FSceneInspectorTransaction> Interaction;
	std::optional<FScenePendingSave> Save;
};

struct FSceneSaveOutcome
{
	bool bCurrentDocument{};
	bool bSucceeded{};
	std::string Path;
	std::string Error;
	double Milliseconds{};
};

// One document instance is shared by GUI and automation. State is observable but only this service mutates history.
class FSceneEditDocument
{
public:
	FSceneEditDocument();
	FSceneEditDocument(const FSceneEditDocument&) = delete;
	FSceneEditDocument& operator=(const FSceneEditDocument&) = delete;
	void Attach(ISceneEditTarget& InTarget, bool bInHistory = true);
	void Detach(FTaskSystem& InTasks);
	ISceneEditTarget& Target() const;
	const FSceneDocumentState& GetState() const;
	FSceneSelection& Selection();
	const FSceneSelection& Selection() const;
	std::string Id() const;
	bool HasHistory() const;
	bool IsDirty() const;
	bool IsBusy() const;
	void SetInteractionState(bool bInBusy, bool bInPreviewDirty);
	void RequireIdle(const std::string& InDocument, std::uint64_t InRevision) const;
	void Reset();
	void Invalidate();
	void SetPath(std::string InPath);
	void MarkSaved(std::uint64_t InEpoch, std::uint64_t InState);
	void AssetsRefreshed();
	void FinishInteraction();
	void CommitEdits(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision,
	                 std::uint64_t InInteraction = 0);
	// Created or newly added lights become main/environment lights only when that selection is empty.
	FSceneHandle CommitCreate(FSceneNode InNode, bool bInAssignLights = true);
	// Structural operations share document history and resource-preserving target operations.
	FSceneHandle CommitDuplicate(FSceneHandle InHandle);
	void CommitRemoveKeepChildren(FSceneHandle InHandle);
	void CommitReparent(FSceneHandle InHandle, std::optional<FSceneHandle> InParent, ESceneReparentMode InMode);
	// Read-only candidates shared by drag feedback and atomic KeepWorld batch submission.
	std::vector<FSceneNodeEdit> PrepareReparent(std::span<const FSceneHandle> InHandles,
	                                            std::optional<FSceneHandle> InParent) const;
	void CommitSettings(FSceneSettings InSettings);
	void CommitDelete();
	void SetClipboardProvider(FSceneClipboardProvider InProvider);
	bool HasClipboardProvider() const;
	FSceneClipboardInfo ClipboardInfo() const;
	FSceneClipboardInfo CopySelection(const std::string& InDocument, std::uint64_t InRevision);
	void PasteClipboard(const std::string& InDocument, std::uint64_t InRevision);
	std::vector<FSceneHandle> SelectedRoots() const;
	void Undo();
	void Redo();
	TAsyncResult<bool> Save(std::string InDestination);
	void PollSave();
	std::optional<FSceneSaveOutcome> TakeSaveOutcome();
	// View-only observer; scoped by the owning plugin and cleared on Detach. Called after history application.
	void SetHistoryObserver(std::function<void(const FSceneHandleMap&)> InObserver);
	void SetSelectionObserver(std::function<void()> InObserver);
	void ReplaceSelection(FSceneSelection InSelection);

private:
	void AssignAddedEnvironment(const FSceneHistoryEntry& InEntry);
	void RestoreHistory(std::size_t InIndex, bool bInAfter);
	void RestoreDeletedSubtree(std::size_t InIndex);
	void RestoreCreatedBatch(std::size_t InIndex, bool bInAfter);
	void CommitClipboardBatch(std::vector<FSceneNode> InNodes);
	std::vector<FSceneNode> PrepareClipboardPaste();
	void RemapHistory(const FSceneHandleMap& InMapping);
	void Append(FSceneHistoryEntry InEntry);
	void NotifyHistory();
	ISceneEditTarget* EditTarget{};
	std::string Identity;
	FSceneDocumentState State;
	FSceneSelection Selected;
	bool bHistory = true;
	bool bBusy{};
	bool bPreviewDirty{};
	bool bAssetRefreshHistory{};
	std::function<void(const FSceneHandleMap&)> Observer;
	std::function<void()> SelectionObserver;
	FSceneHandleMap Remapped;
	std::optional<FSceneSaveOutcome> SaveOutcome;
	FSceneClipboardProvider ClipboardProvider;
	std::optional<FSceneClipboardSnapshot> ClipboardSnapshot;
	std::uint64_t AssetGeneration{};
};
} // namespace Hyperion
