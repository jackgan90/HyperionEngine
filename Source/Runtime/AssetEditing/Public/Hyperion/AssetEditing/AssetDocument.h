#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Textures/TextureAsset.h"

namespace Hyperion
{
struct FModelPrimitiveInfo;

// Base open support only; preview availability and field edit permissions have separate owners.
bool SupportsAssetDocument(const FRecordDescriptor& InType);

// Only the edited fields enter history; bulk payloads are immutable and shared.
class FAssetEditDocument
{
public:
	explicit FAssetEditDocument(std::shared_ptr<const FLoadedAsset> InAsset);
	const FArchiveNode& Get(std::string_view InField) const;
	void Set(std::string InField, FArchiveNode InValue, std::uint64_t InInteraction = 0);
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
	bool IsEditing() const;
	std::string Error;

private:
	friend class FAssetEditWorkflow;
	friend void CommitAssetField(FAssetEditDocument&, const FRecordMemberIdentity&, FArchiveNode, std::uint64_t);
	friend void SetModelPrimitives(FAssetEditDocument&, const std::vector<FModelPrimitiveInfo>&, std::uint64_t);

	enum class ETarget
	{
		Field,
		Root
	};

	void Apply(const FRecordMemberIdentity& InField, FArchiveNode InValue, std::uint64_t InInteraction,
	           ETarget InTarget = ETarget::Field);
	bool bEditing{};

	struct FEdit
	{
		FRecordMemberIdentity Field;
		ETarget Target{};
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
bool IsAssetPathReadOnly(const FAssetService& InAssets, const std::filesystem::path& InPath);
bool CanEditTextureEncoding(const FTextureAsset& InTexture);
FArchiveNode RebuildTextureEncodingDraft(const FArchiveNode& InDraft, EMaterialTextureEncoding InEncoding);
} // namespace Hyperion
