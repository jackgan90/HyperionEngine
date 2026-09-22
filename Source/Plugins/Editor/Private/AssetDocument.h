#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Textures/TextureAsset.h"

namespace Hyperion
{
// Only the edited fields enter history; bulk payloads are immutable and shared.
class FAssetEditorDocument
{
public:
	explicit FAssetEditorDocument(std::shared_ptr<const FLoadedAsset> InAsset);
	const FArchiveNode& Get(std::string_view InField) const;
	void Set(std::string InField, FArchiveNode InValue, std::uint64_t InInteraction = 0, bool bInAffectsPreview = true);
	void FinishInteraction();
	void CancelInteraction(std::uint64_t InInteraction = 0);
	bool Undo();
	bool Redo();
	bool CanUndo() const;
	bool CanRedo() const;
	bool IsDirty() const;
	std::uint64_t Generation() const;
	std::uint64_t PreviewGeneration() const;
	const FArchiveNode& Snapshot() const;
	const FLoadedAsset& Loaded() const;
	void Save(FAssetService& InAssets);
	std::optional<FAssetSaveResult> PollSave();
	bool IsSaving() const;
	std::string Error;

private:
	struct FEdit
	{
		std::string Field;
		FArchiveNode Before;
		FArchiveNode After;
		std::uint64_t BeforeState{};
		std::uint64_t AfterState{};
		std::uint64_t Interaction{};
		bool bAffectsPreview{};
	};

	std::shared_ptr<const FLoadedAsset> Asset;
	FArchiveNode Draft;
	std::vector<FEdit> History;
	std::size_t Cursor{};
	std::uint64_t State{};
	std::uint64_t SavedState{};
	std::uint64_t NextState{};
	std::uint64_t Revision = 1;
	std::uint64_t PreviewRevision = 1;
	std::uint64_t ActiveInteraction{};
	std::uint64_t SubmittedState{};
	std::optional<TAsyncResult<FAssetSaveResult>> PendingSave;
};

void ShareAssetBulk(FArchiveNode& InNode);
FArchiveNode RebuildTextureEncodingDraft(const FArchiveNode& InDraft, EMaterialTextureEncoding InEncoding);
} // namespace Hyperion
