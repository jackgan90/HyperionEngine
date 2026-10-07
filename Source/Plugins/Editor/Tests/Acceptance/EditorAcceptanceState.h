#pragma once
#if !HYP_BUILD_TESTING
#error Acceptance scenario state is only available in test-enabled builds
#endif
#include "AcceptanceBounds.h"
#include "AcceptanceContexts.h"
#include "AcceptanceScenarioContexts.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneViewport.h"
#include "Hyperion/Scene/Scene.h"
#include "ReparentAcceptanceContext.h"
#include <filesystem>
#include <semaphore>

namespace Hyperion
{
struct FEditorAcceptanceState
{
	FTransformAcceptanceContext Transform;
	FShortcutAcceptanceContext Shortcut;
	FLightPriorityAcceptanceContext LightPriority;
	FDepthAcceptanceContext Depth;
	FMultiSelectionAcceptanceContext MultiSelection;
	TAcceptanceState<EGizmoState> Gizmo;
	TAcceptanceState<EReparentSelectionState> ReparentSelection;
	TAcceptanceState<EReparentKeyboardState> ReparentKeyboard;
	TAcceptanceState<EReparentDocumentState> ReparentDocument;
	TAcceptanceState<EReparentDragState> ReparentDrag;
	FOutlineAcceptanceContext Outline;
	TAcceptanceState<EPlacementDocumentState> PlacementDocument;
	TAcceptanceState<EPlacementCancelState> PlacementCancel;
	TAcceptanceState<EPlacementDragState> PlacementDrag;
	FInteractionAcceptanceContext Interaction;
	FMovementAcceptanceContext Movement;
	FWheelAcceptanceContext Wheel;
	FAssetScenarioContext Asset;
	FDocumentAcceptanceContext Document;
	FViewAcceptanceContext View;
	FImportAcceptanceContext Import;
	FLogAcceptanceContext Log;
	FCapturePreferenceAcceptanceContext CapturePreference;
	FCaptureAcceptanceContext Capture;
	FContentAcceptanceContext Content;
	FRenderControlsAcceptanceContext RenderControls;
	FClipboardAcceptanceContext Clipboard;
	FFramingAcceptanceContext Framing;
	TAcceptanceState<EDeletionState> Deletion;
	TAcceptanceState<EPlacementMenuState> PlacementMenu;
	TAcceptanceState<EPlacementMarkerState> PlacementMarker;
	TAcceptanceState<EPickingSceneState> PickingScene;
	TAcceptanceState<EPickingState> Picking;
	FModelPlacementAcceptanceContext ModelPlacement;
	TAcceptanceState<EModelPlacementHistoryState> ModelPlacementHistory;

	struct FAssetPreviewExercise
	{
		static constexpr unsigned PositionControlSettleFrames = 4;
		static constexpr unsigned DragSampleFrames = 48;
		static constexpr unsigned HistorySettleFrames = 3;
		TAcceptanceState<EAssetPreviewState> Progress;
		FAcceptanceFrameWait PositionControlSettle{PositionControlSettleFrames};
		FAcceptanceFrameWait DragSample{DragSampleFrames};
		FAcceptanceFrameWait HistorySettle{HistorySettleFrames};
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
		TAcceptanceState<ERasterState> Progress;
		FAcceptanceClick Click;
		FAcceptanceFrameWait ChoiceObservation{2};
		FRenderSettings Initial;
		FRenderSettings Before;
		std::uint64_t SceneRevision{};
		std::size_t History{};
	};

	FRasterOptionExercise RasterOptionExercise;

	struct FViewportChoiceExercise
	{
		unsigned Case{};
		TAcceptanceState<EViewportChoiceState> Progress;
		FAcceptanceClick Click;
		FAcceptanceFrameWait ChoiceObservation{2};
		FSceneViewportOptions Initial;
		FSceneViewportOptions Before;
		std::uint64_t SceneRevision{};
		std::uint64_t RenderRevision{};
		std::size_t HistoryCursor{};
		std::size_t HistorySize{};
	};

	FViewportChoiceExercise ViewportChoiceExercise;

	std::uint32_t PlacementCancelCase{};
	std::uint32_t PlacementMarkerCase{};
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
	std::optional<EAssetState> AssetLoggedState;
	bool bAssetsVerified{};
	bool bLogVerified{};
	FReparentAcceptanceContext ReparentExercise;
	bool bReparentVerified{};
	std::uint64_t ReparentExerciseRevision{};
	std::size_t ReparentExerciseHistory{};
	FVec4 ReparentSceneBounds;
	std::map<std::string, FVec4> ReparentRowBounds;
	std::vector<FSceneHandle> FramingObjects;
	std::uint64_t FramingRevision{};
	FSceneCameraView FramingBefore;
	FSceneCameraView FramingMultiple;
	FBytes FramingSnapshot;
	bool bFramingVerified{};
	std::size_t ClipboardExerciseCount{};
	bool bClipboardVerified{};
	std::uint32_t PlacementExerciseType{};
	std::size_t PlacementExerciseBaseNodes{};
	std::size_t PlacementExerciseBaseHistory{};
	std::uint64_t PlacementExerciseBaseState{};
	FVec3 PlacementExercisePosition;
	std::vector<std::string> PlacementExerciseIds;
	bool bPlacementVerified{};
	bool bModelPlacementVerified{};
	unsigned ModelPlacementCase{};
	std::size_t ModelPlacementBaseHistory{};
	std::size_t ModelPlacementBaseNodes{};
	FVec3 ModelPlacementPosition;
	std::vector<std::string> ModelPlacementIds;
	FMat4 GizmoExerciseBefore;
	FMat4 GizmoExerciseAfter;
	FSceneCameraView GizmoExerciseCamera;
	bool bGizmoVerified{};
	std::vector<FSceneHandle> OutlineExerciseObjects;
	FSceneHandle OutlineExerciseWall;
	bool bOutlinesVerified{};
	bool bMultiSelectionVerified{};
	std::vector<FSceneHandle> MultiSelectionObjects;
	std::map<std::string, FVec4> MultiSelectionRows;
	std::vector<FSceneHandle> ShortcutObjects;
	std::map<std::string, FVec4> ShortcutTreeToggles;
	std::uint64_t ShortcutRevision{};
	std::string ShortcutObjectId;
	std::string ShortcutDocumentPath;
	bool bSelectionShortcutsVerified{};
	std::array<FMat4, 2> MultiSelectionInitial;
	std::array<FMat4, 2> MultiSelectionFinal;
	FVec4 PickingLightBounds;
	FSceneHandle PickingNear;
	FSceneHandle PickingFar;
	FSceneHandle PickingPreview;
	bool bPickingVerified{};
	bool bContentVerified{};
	std::shared_ptr<std::binary_semaphore> ContentSaveGate;
	std::vector<std::byte> ContentSaveOriginal;
	bool bImportVerified{};
	bool bMovementVerified{};
	bool bMovementGateVerified{};
	bool bRightReleaseVerified{};
	bool bLookVerified{};
	bool bDollyVerified{};
	bool bSpeedVerified{};
	bool bInputIsolationVerified{};
	FSceneHandle DeletionExerciseHandle;
	std::string DeletionExerciseId;
	FSceneNode ExerciseOriginal;
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
	FAcceptanceBounds Bounds;
	std::string ContentRevealPath;
};
} // namespace Hyperion
