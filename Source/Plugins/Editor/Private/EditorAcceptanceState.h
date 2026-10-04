#pragma once
#if !HYP_BUILD_TESTING
#error Acceptance scenario state is only available in test-enabled builds
#endif
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneViewport.h"
#include "Hyperion/Scene/Scene.h"
#include <filesystem>
#include <semaphore>

namespace Hyperion
{
struct FEditorAcceptanceState
{
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

	struct FViewportChoiceExercise
	{
		unsigned Case{};
		unsigned Step{};
		unsigned Wait{};
		FSceneViewportOptions Initial;
		FSceneViewportOptions Before;
		std::uint64_t SceneRevision{};
		std::uint64_t RenderRevision{};
		std::size_t HistoryCursor{};
		std::size_t HistorySize{};
	};

	FViewportChoiceExercise ViewportChoiceExercise;

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
	FVec4 EditMenuBounds;
	FVec4 PreferencesMenuBounds;
	FVec4 CapturePreferenceBounds;
	FVec4 CaptureHudPreferenceBounds;
	FVec4 PreferencesCloseBounds;
	FVec4 CaptureButtonBounds;
	std::array<FVec4, 3> GizmoButtonBounds;
	FVec4 SaveSwitchBounds;
	FVec4 DiscardChangesBounds;
	FVec4 CancelChangesBounds;
	FVec4 DiscardTitleBounds;
	std::map<std::string, FVec4> ContentTileBounds;
	FVec4 ContentClickBounds;
	FVec4 ImportMenuBounds{};
	FVec4 FileMenuBounds;
	FVec4 OpenMenuBounds;
	FVec4 SponzaBounds;
	FVec4 OpenButtonBounds;
	FVec4 CancelButtonBounds;
	std::map<std::string, FVec4> InspectionBounds;
	std::string ContentRevealPath;
};
} // namespace Hyperion
