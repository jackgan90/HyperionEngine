#pragma once
#include "AssetDocument.h"
#include "Hyperion/GuiRenderer/GuiRenderer.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"

namespace Hyperion
{
class FAssetWorkspace
{
public:
	FAssetWorkspace(FAssetService& InAssets, FTaskSystem& InTasks, FRenderSession& InSession,
	                FRHICapabilities InCapabilities);
	~FAssetWorkspace();
	void Open(const std::filesystem::path& InPath, std::string_view InIdentity = {});
	void CloseAll();
	bool HasDocuments() const;
	bool IsClosePending() const;
	void CancelCloseDialog();
	void SuspendInput(std::span<const FInputEvent> InEvents);
	bool HasActive() const;
	bool IsDirty() const;
	bool IsSaving() const;
	bool HasPendingEdits() const;
	bool CanUndo() const;
	bool CanRedo() const;
	bool HasActiveInteraction() const;
	void Undo();
	void Redo();
	void SaveActive();
	void SaveAll();
	std::vector<FAssetSaveResult> Poll();
	void RefreshDependencies(std::span<const FAssetSaveResult> InSaved);
	void RefreshIndex();
	void BeginFrame();
	void DrawTabs(FGui& InGui, float InDelta, std::span<const FInputEvent> InEvents);
	void DrawProperties(FGui& InGui);
	void DrawCloseDialog(FGui& InGui);
	void PrepareFrame();
	void Build(FRenderGraph& InGraph, std::vector<FGuiTextureBinding>& OutTextures);
	const FAssetEditorDocument* ActiveDocument() const;
	bool IsPreviewReady() const;
	std::string ActiveStatus() const;
	FVec4 ObservedBounds(std::string_view InId) const;
	void RevealProperty(std::string InId);

private:
	struct FEntry;

	struct FReferenceSelection
	{
		std::uint64_t Generation{};
		std::string Type;
		std::optional<ETextureDimension> Dimension;
		TAsyncResult<FAssetGraph> Request;
		std::string Field;
		FArchiveNode Candidate;
	};

	struct FPrepared
	{
		std::shared_ptr<const FLoadedAsset> Root;
		std::shared_ptr<const FSceneModelData> Model;
		std::shared_ptr<const FSceneModelData> SecondModel;
		std::set<std::string> Dependencies;
		std::array<std::shared_ptr<const FTextureAsset>, 3> SkyProducts;
		std::string Error;
	};

	struct FTextureView
	{
		std::size_t Mip{};
		std::size_t Face{};
		std::size_t Channel{};
		float Exposure{};
		float Zoom = 1;
		FVec2 Pan{};
		FVec2 LastPointer{};
		bool bFit = true;
		bool bDragging{};
		bool bChecker = true;
		std::uint64_t Revision = 1;
		std::uint64_t PreparedRevision{};
		std::optional<TAsyncResult<FTextureAsset>> Pending;
		FRenderTargetSource Target;
		FRenderTargetSource PendingTarget;
		std::optional<TAsyncResult<bool>> Upload;
		std::uint64_t TargetRevision{};
	};

	struct FEntry
	{
		std::filesystem::path Path;
		std::string Identity;
		FAssetRequest Load;
		std::shared_ptr<FAssetEditorDocument> Document;
		std::optional<TAsyncResult<std::shared_ptr<FAssetEditorDocument>>> Initialization;
		std::optional<TAsyncResult<FArchiveNode>> EncodingEdit;
		std::optional<FReferenceSelection> ReferenceEdit;
		bool bSaveRequested{};
		std::uint64_t EncodingGeneration{};
		std::uint64_t GuiInteraction{};
		bool bReadOnly{};
		std::optional<TAsyncResult<FPrepared>> Pending;
		FCancellationToken Cancellation;
		std::uint64_t RequestedGeneration{};
		std::uint64_t PreparedGeneration{};
		std::uint64_t TextureId{};
		std::size_t Shape{};
		std::size_t SelectedNode{};
		std::size_t SelectedPrimitive{};
		bool bActivate = true;
		bool bVisible{};
		bool bCameraInitialized{};
		bool bCloseAfterSave{};
		bool bClosePending{};
		std::string Error;
		std::unique_ptr<FRenderSession> PreviewSession;
		std::unique_ptr<FSceneInstance> Scene;
		std::unique_ptr<FSceneRenderPipeline> Pipeline;
		std::shared_ptr<const FLoadedAsset> Preview;
		std::shared_ptr<const FSceneModelData> PreviewModel;
		std::array<std::shared_ptr<const FTextureAsset>, 3> SkyProducts;
		std::set<std::string> Dependencies;
		FSceneCameraView Camera;
		FSceneCameraController Navigation;
		FRenderTargetSource Target;
		FSize Size{};
		FGuiImageRegion Region;
		std::shared_ptr<const FSceneFrameSeed> Seed;
		FTextureView Texture;

		bool HasPendingEdit() const
		{
			return EncodingEdit.has_value() || ReferenceEdit.has_value();
		}

		float Exposure = 1;
		float YawDegrees{};
	};

	FPrepared Prepare(const FLoadedAsset& InLoaded, FArchiveNode InDraft, std::size_t InShape,
	                  FCancellationToken InCancellation, std::shared_ptr<const FSceneModelData> InExisting);
	void PollEntry(FEntry& InEntry);
	void PollReferenceEdit(FEntry& InEntry);
	void CommitReferenceEdit(FEntry& InEntry, std::string InField, FArchiveNode InCandidate);
	void Publish(FEntry& InEntry, const FPrepared& InPrepared);
	void Close(FEntry& InEntry);
	void DrawPreview(FGui& InGui, FEntry& InEntry, float InDelta, std::span<const FInputEvent> InEvents);
	void DrawTexture(FGui& InGui, FEntry& InEntry, std::span<const FInputEvent> InEvents);
	void DrawTextureControls(FGui& InGui, FEntry& InEntry, const FTextureAsset& InTexture);
	void PollTextureDisplay(FEntry& InEntry, const std::shared_ptr<const FTextureAsset>& InTexture);
	void DrawTextureCanvas(FGui& InGui, FEntry& InEntry, const FTextureAsset& InTexture,
	                       std::span<const FInputEvent> InEvents);
	void DrawTextureProperties(FGui& InGui, FEntry& InEntry);
	void DrawModelProperties(FGui& InGui, FEntry& InEntry);
	void DrawModelNodes(FGui& InGui, FEntry& InEntry);
	void DrawModelPrimitives(FGui& InGui, FEntry& InEntry, const FModelAsset& InModel);
	void DrawSkyProperties(FGui& InGui, FEntry& InEntry);
	void DrawMaterialProperties(FGui& InGui, FEntry& InEntry);
	bool EditReference(FGui& InGui, const char* InLabel, FAssetRef& InReference, std::string_view InType,
	                   std::optional<ETextureDimension> InDimension = {});
	bool EditMaterialValue(FGui& InGui, const std::string& InId, FMaterialAssetValue& InValue);
	void DrawMaterialParameter(FGui& InGui, FEntry& InEntry, FMaterialAsset& InMaterial,
	                           const FMaterialAssetParameter& InParameter);
	void ObserveProperty(FGui& InGui, const std::string& InId);
	void EditField(FGui& InGui, FEntry& InEntry, const char* InField, const std::function<bool()>& InWidget,
	               const std::function<FArchiveNode()>& InValue, bool bInAffectsPreview = true);
	FAssetService& Assets;
	FTaskSystem& Tasks;
	FRenderSession& Session;
	FRHICapabilities Capabilities;
	std::vector<std::unique_ptr<FEntry>> Entries;
	std::vector<FAssetRef> AssetIndex;

	struct FReferenceChoices
	{
		std::vector<std::string> Labels;
		std::vector<FAssetRef> References;
		std::map<std::string, std::size_t, std::less<>> Indices;
	};

	std::map<std::string, FReferenceChoices, std::less<>> ReferenceChoices;
	std::map<std::string, FVec4, std::less<>> Bounds;
	std::string RevealControl;
	FEntry* Active{};
	FEntry* Closing{};

	std::uint64_t NextTexture = 100;
	bool bRequestClose{};
	bool bCloseModal{};
};
} // namespace Hyperion
