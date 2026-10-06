#pragma once
#include "AcceptanceInput.h"
#include "Hyperion/Renderer/SceneNavigation.h"

namespace Hyperion
{
struct FInteractionAcceptanceContext
{
	TAcceptanceState<EInteractionState> Progress;
	FAcceptanceClick Click;
	FAcceptanceFrameWait IsolationObservation{6};
	FAcceptanceFrameWait HiddenViewportObservation{4};
	EAcceptanceClickDelay DollyMenuDelay{EAcceptanceClickDelay::Settle};
	EAcceptanceClickDelay CancelDialogDelay{EAcceptanceClickDelay::Settle};
	FSceneCameraPose LookBefore;
	FSceneCameraPose DollyBefore;
	FSceneCameraPose IsolationBefore;
	float IsolationSpeed{};
};

struct FMovementAcceptanceContext
{
	static constexpr unsigned GateObservationFrames = 6;
	static constexpr unsigned MovementSampleFrames = 8;
	static constexpr unsigned ReleaseObservationFrames = 6;
	TAcceptanceState<EMovementState> Progress;
	FAcceptanceFrameWait GateObservation{GateObservationFrames};
	FAcceptanceFrameWait MovementSample{MovementSampleFrames};
	FAcceptanceFrameWait ReleaseObservation{ReleaseObservationFrames};
	FSceneCameraPose Before;
};

struct FWheelAcceptanceContext
{
	static constexpr unsigned MovementSampleFrames = 6;
	TAcceptanceState<EWheelState> Progress;
	FAcceptanceFrameWait MovementSample{MovementSampleFrames};
	FSceneCameraPose Before;
	float InitialSpeed{};
};

struct FTransformAcceptanceContext
{
	TAcceptanceState<ETransformState> Progress;
	FAcceptanceClick Click;
	FAcceptanceFrameWait TextCommitObservation{2};
};

struct FLightPriorityAcceptanceContext
{
	TAcceptanceState<ELightPriorityState> Progress;
	FAcceptanceClick Click;
	FAcceptanceFrameWait TextCommitObservation{2};
};

struct FDepthAcceptanceContext
{
	TAcceptanceState<EDepthState> Progress;
	FAcceptanceClick Click;
};

struct FRenderControlsAcceptanceContext
{
	TAcceptanceState<ERenderControlsState> Progress;
	FAcceptanceClick Click;
	FAcceptanceFrameWait NarrowHudObservation{8};
	FAcceptanceFrameWait VisibilityHudObservation{8};
	bool bCollectionControlsPrepared{};
};

struct FDocumentAcceptanceContext
{
	TAcceptanceState<EDocumentState> Progress;
	FAcceptanceClick Click;
};

struct FViewAcceptanceContext
{
	TAcceptanceState<EViewState> Progress;
	FAcceptanceClick Click;
	bool bCameraRestored{};
	bool bInitialViewCaptured{};
	bool bApplyBaselineCaptured{};
};

struct FContentAcceptanceContext
{
	TAcceptanceState<EContentState> Progress;
	FAcceptanceClick Click;
};

struct FCaptureAcceptanceContext
{
	TAcceptanceState<ECaptureState> Progress;
	FAcceptanceClick Click;
};

struct FCapturePreferenceAcceptanceContext
{
	TAcceptanceState<ECapturePreferenceState> Progress;
	FAcceptanceClick Click;
	FAcceptanceFrameWait PreferenceObservation{3};
	bool bCaptureBeforeToggle{};
};

enum class ESourceTextEntryPhase
{
	EnterText,
	AwaitTextCommit
};

struct FImportAcceptanceContext
{
	TAcceptanceState<EImportState> Progress;
	FAcceptanceClick Click;
	TAcceptanceState<ESourceTextEntryPhase> SourceEntry;
	FAcceptanceFrameWait SourceTextCommit{4};
	FAcceptanceTextInput DraftName{EAcceptanceTextCommit::KeepEditing, 4};
	bool bImportResultCaptured{};
	bool bRefreshResultCaptured{};
};

enum class ELogObservationPhase
{
	EmitRecords,
	VerifyCapturedHistory
};

struct FLogAcceptanceContext
{
	TAcceptanceState<ELogState> Progress;
	FAcceptanceClick Click;
	TAcceptanceState<ELogObservationPhase> Observation;
	FAcceptanceFrameWait RecordDelay{4};
	FAcceptanceFrameWait HistoryDelay{9};
	bool bClosedPanelRecordWritten{};
};

struct FModelPlacementAcceptanceContext
{
	static constexpr unsigned NextCaseSettleFrames = 24;
	TAcceptanceState<EModelPlacementState> Progress;
	FAcceptanceFrameWait NextCaseSettle;
};

struct FFramingAcceptanceContext
{
	TAcceptanceState<EFramingState> Progress;
	FAcceptanceInputCadence Cadence;
};

struct FClipboardAcceptanceContext
{
	TAcceptanceState<EClipboardState> Progress;
	FAcceptanceInputCadence Cadence;
};

struct FShortcutAcceptanceContext
{
	TAcceptanceState<EShortcutState> Progress;
	FAcceptanceInputCadence Cadence;
};

struct FMultiSelectionAcceptanceContext
{
	TAcceptanceState<EMultiSelectionState> Progress;
	FAcceptanceInputCadence Cadence;
};

struct FOutlineAcceptanceContext
{
	TAcceptanceState<EOutlineState> Progress;
	FAcceptanceFrameWait CaptureObservation{3};
	bool bCapturePrepared{};
};

struct FAssetTabCloseInput
{
	static constexpr unsigned HoverSettleFrames = 30;
	FAcceptanceFrameWait Hover{HoverSettleFrames};
	FAcceptanceClick Click;
};

enum class EAssetWindowNamePhase
{
	SelectAll,
	TypeName,
	AwaitTextCommit
};

struct FAssetWindowNameInput
{
	TAcceptanceState<EAssetWindowNamePhase> Progress;
	FAcceptanceFrameWait SelectionObservation{2};
	FAcceptanceFrameWait TextCommit{3};
};

enum class EAssetPanelDragPhase
{
	AwaitBaseline,
	ObserveHeldDrag
};

struct FAssetPanelDragInput
{
	TAcceptanceState<EAssetPanelDragPhase> Progress;
	FAcceptanceFrameWait TitleHover{3};
	FAcceptanceFrameWait BaselineObservation{6};
	FAcceptanceFrameWait HeldDragObservation{8};
	FAcceptanceFrameWait ReleaseObservation{3};
};

struct FAssetScenarioContext
{
	static constexpr unsigned PreviewDiagnosticFrames = 120;
	static constexpr unsigned CloseDiagnosticFrames = 60;
	TAcceptanceState<EAssetState> Progress;
	FAcceptanceClick Click;
	FAcceptanceFrameObservation PreviewObservation;
	FAcceptanceFrameObservation ModelCloseRequestObservation;
	FAcceptanceFrameObservation ModelCloseObservation;
	FAcceptanceTextInput NameInput;
	FAcceptanceTextInput RoughnessInput{EAcceptanceTextCommit::Enter};
	FAcceptanceTextInput TintInput{EAcceptanceTextCommit::Enter};
	FAcceptanceTextInput DiscardNameInput{EAcceptanceTextCommit::Enter};
	FAssetWindowNameInput WindowHistoryName;
	FAssetWindowNameInput WindowCloseName;
	FAssetTabCloseInput TabClose;
	FAssetTabCloseInput ModelTabClose;
	FAssetPanelDragInput PanelDrag;
	FAcceptanceFrameWait ResizeObservation{6};
	FAcceptanceFrameWait AssetMinimizeObservation{6};
	FAcceptanceFrameWait OwnerMinimizeObservation{6};
	FAcceptanceFrameWait OwnerRestoreObservation{6};
};
} // namespace Hyperion
