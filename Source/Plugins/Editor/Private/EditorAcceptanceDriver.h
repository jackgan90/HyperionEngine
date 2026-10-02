#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Scene/Scene.h"
#include <filesystem>
#include <semaphore>

namespace Hyperion
{
class FEditorPlugin;
enum class EEditorAssetAcceptancePhase
{
	MainWindow,
	NameAndSave,
	AssetProperties,
	AssetWindowLifecycle
};

enum class EEditorAcceptanceWindow
{
	Main,
	Asset
};

class FEditorAcceptanceDriver
{
public:
	void CollectInput(FEditorPlugin& InEditor, std::vector<FInputEvent>& InEvents,
	                  std::vector<FInputEvent>& InAssetEvents);
	bool IsComplete(const FEditorPlugin& InEditor) const;
	bool ShouldCapture(const FEditorPlugin& InEditor) const;
	void CheckCompletion(FEditorPlugin& InEditor) const;
	void CheckTimeout(const FEditorPlugin& InEditor, double InElapsed) const;
	void CheckGui(FEditorPlugin& InEditor, const FGuiDrawData& InData) const;
	void CheckAssetWindow(FEditorPlugin& InEditor);
	void DrawPanels(FEditorPlugin& InEditor) const;
	EEditorAssetAcceptancePhase AssetPhase() const;
	EEditorAcceptanceWindow AssetInputWindow() const;

	struct FAssetPreviewExercise
	{
		unsigned Step{};
		unsigned Frames{};
		FVec4 Canvas;
		FVec2 Pointer;
		std::string Before;
		std::string After;
		bool bSawPreparing{};
		bool bSawReady{};
	};

	struct FRasterOptionExercise
	{
		unsigned Case{};
		unsigned Step{};
		FRenderSettings Initial;
		FRenderSettings Before;
		std::uint64_t SceneRevision{};
		std::size_t History{};
	};

	FRasterOptionExercise RasterOptionExercise;

	std::uint32_t PlacementMenuStep{};
	std::uint32_t PlacementCancelCase{};
	std::uint32_t PlacementMarkerCase{};
	std::uint32_t PlacementMarkerStep{};
	std::array<FMat4, 2> PlacementMarkerTransforms;
	bool bInitialCapturePreference{};
	bool bInitialCaptureHudPreference{};
	std::filesystem::path PlacementCapture;
	std::filesystem::path OutlineCapture;
	bool bRenderControlsVerified{};
	FAssetPreviewExercise AssetPreviewExercise;
	bool bPendingAssetEditChecked{};
	std::uint64_t AssetExerciseFrames{};
	std::shared_ptr<const FBytes> AssetExerciseSavedBytes;
	float AssetExerciseScale{};
	FSceneCameraView AssetExerciseSceneCamera;
	FRenderSettings AssetRasterInitial;
	std::uint64_t AssetRasterSceneRevision{};
	std::size_t AssetRasterHistory{};
	FVec4 AssetExercisePanelStart{};
	FVec2 AssetExercisePointer{};
	std::shared_ptr<const FSceneModelData> AssetExerciseModel;
	float AssetExerciseRoughness{};
	std::size_t AssetExerciseIndex{};
	std::string AssetExerciseOriginalName;
	int AssetExerciseLoggedStep = -1;
	bool bAssetsVerified{};
	bool bLogVerified{};
	unsigned ReparentExerciseStep{};
	unsigned ReparentExerciseCase{};
	unsigned ReparentSelectionStep{};
	unsigned ReparentKeyboardStep{};
	bool bReparentVerified{};
	std::vector<FSceneHandle> ReparentExerciseNodes;
	std::vector<std::string> ReparentExerciseIds;
	std::vector<FMat4> ReparentExerciseWorlds;
	std::uint64_t ReparentExerciseRevision{};
	std::size_t ReparentExerciseHistory{};
	std::vector<FSceneHandle> FramingObjects;
	unsigned FramingStep{};
	unsigned FramingWait{};
	std::uint64_t FramingRevision{};
	FSceneCameraView FramingBefore;
	FSceneCameraView FramingMultiple;
	FBytes FramingSnapshot;
	bool bFramingVerified{};
	unsigned ClipboardExerciseStep{};
	unsigned ClipboardExerciseWait{};
	std::size_t ClipboardExerciseCount{};
	bool bClipboardVerified{};
	std::uint32_t PlacementExerciseStep{};
	std::uint32_t PlacementExerciseType{};
	std::size_t PlacementExerciseBaseNodes{};
	std::size_t PlacementExerciseBaseHistory{};
	std::uint64_t PlacementExerciseBaseState{};
	FVec3 PlacementExercisePosition;
	std::vector<std::string> PlacementExerciseIds;
	bool bPlacementVerified{};
	bool bModelPlacementVerified{};
	unsigned ModelPlacementStep{};
	unsigned ModelPlacementCase{};
	std::size_t ModelPlacementBaseHistory{};
	std::size_t ModelPlacementBaseNodes{};
	FVec3 ModelPlacementPosition;
	std::vector<std::string> ModelPlacementIds;
	std::uint32_t GizmoExerciseStep{};
	FMat4 GizmoExerciseBefore;
	FMat4 GizmoExerciseAfter;
	FSceneCameraView GizmoExerciseCamera;
	bool bGizmoVerified{};
	std::vector<FSceneHandle> OutlineExerciseObjects;
	FSceneHandle OutlineExerciseWall;
	unsigned OutlineExerciseStep{};
	unsigned OutlineExerciseWait{};
	bool bOutlinesVerified{};
	unsigned MultiSelectionStep{};
	unsigned MultiSelectionWait{};
	bool bMultiSelectionVerified{};
	std::vector<FSceneHandle> MultiSelectionObjects;
	std::map<std::string, FVec4> MultiSelectionRows;
	std::vector<FSceneHandle> ShortcutObjects;
	unsigned ShortcutStep{};
	unsigned ShortcutWait{};
	std::uint64_t ShortcutRevision{};
	std::string ShortcutObjectId;
	std::string ShortcutDocumentPath;
	bool bSelectionShortcutsVerified{};
	std::array<FMat4, 2> MultiSelectionInitial;
	std::array<FMat4, 2> MultiSelectionFinal;
	unsigned PickingExerciseStep{};
	unsigned PickingSceneStep{};
	FVec4 PickingLightBounds;
	FSceneHandle PickingNear;
	FSceneHandle PickingFar;
	FSceneHandle PickingPreview;
	bool bPickingVerified{};
	bool bContentVerified{};
	std::shared_ptr<std::binary_semaphore> ContentSaveGate;
	std::vector<std::byte> ContentSaveOriginal;
	bool bImportVerified{};
	std::uint32_t ExerciseStep{};
	std::uint32_t ExerciseWait{};
	std::uint32_t ExerciseMovementStep{};
	std::uint32_t ExerciseWheelStep{};
	float ExerciseSpeed{};
	bool bExerciseMouseDown{};
	bool bMovementVerified{};
	bool bMovementGateVerified{};
	bool bRightReleaseVerified{};
	bool bLookVerified{};
	bool bDollyVerified{};
	bool bSpeedVerified{};
	bool bInputIsolationVerified{};
	FSceneCameraPose ExercisePose;
	unsigned DeletionExerciseStep{};
	FSceneHandle DeletionExerciseHandle;
	std::string DeletionExerciseId;
	FSceneNode ExerciseOriginal;
	std::uint32_t TransformExerciseStep{};
	std::uint32_t LightPriorityExerciseStep{};
	std::uint32_t DepthExerciseStep{};
	FMat4 DepthExerciseFrozenView;
	FSceneCameraView DepthExerciseCamera;
	std::uint64_t TransformDragReadyAt{};
	FMat4 ExerciseTransformResult;
	bool bDocumentVerified{};
	bool bViewsVerified{};
	FSceneCameraView ExerciseInitialView;
	FSceneCameraView ExerciseEditorView;
};
} // namespace Hyperion
