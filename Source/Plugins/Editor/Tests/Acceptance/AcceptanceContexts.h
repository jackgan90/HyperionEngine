#pragma once
#include "AcceptanceTransition.h"
#include <optional>

namespace Hyperion
{
enum class EGizmoAction
{
	PrepareModel,
	PressMode,
	ReleaseMode,
	BeginDrag,
	MoveDrag,
	VerifyPreviewAndRelease,
	VerifyHistory,
	BeginCancelledDrag,
	MoveCancelledDrag,
	PressEscape,
	ReleaseEscape,
	VerifyCancellation,
	BeginFocusDrag,
	MoveFocusDrag,
	LoseFocus,
	VerifyFocusCommit,
};

struct FGizmoContext
{
	unsigned CaseIndex{};
	EGizmoAction Action{};
};

inline std::optional<FGizmoContext> FindGizmoContext(EGizmoState InState)
{
	switch (InState)
	{
		case EGizmoState::PositionPrepareModel:
			return FGizmoContext{0, EGizmoAction::PrepareModel};
		case EGizmoState::PositionPressMode:
			return FGizmoContext{0, EGizmoAction::PressMode};
		case EGizmoState::PositionReleaseMode:
			return FGizmoContext{0, EGizmoAction::ReleaseMode};
		case EGizmoState::PositionBeginDrag:
			return FGizmoContext{0, EGizmoAction::BeginDrag};
		case EGizmoState::PositionMoveDrag:
			return FGizmoContext{0, EGizmoAction::MoveDrag};
		case EGizmoState::PositionVerifyPreviewAndRelease:
			return FGizmoContext{0, EGizmoAction::VerifyPreviewAndRelease};
		case EGizmoState::PositionVerifyHistory:
			return FGizmoContext{0, EGizmoAction::VerifyHistory};
		case EGizmoState::PositionBeginCancelledDrag:
			return FGizmoContext{0, EGizmoAction::BeginCancelledDrag};
		case EGizmoState::PositionMoveCancelledDrag:
			return FGizmoContext{0, EGizmoAction::MoveCancelledDrag};
		case EGizmoState::PositionPressEscape:
			return FGizmoContext{0, EGizmoAction::PressEscape};
		case EGizmoState::PositionReleaseEscape:
			return FGizmoContext{0, EGizmoAction::ReleaseEscape};
		case EGizmoState::PositionVerifyCancellation:
			return FGizmoContext{0, EGizmoAction::VerifyCancellation};
		case EGizmoState::PositionBeginFocusDrag:
			return FGizmoContext{0, EGizmoAction::BeginFocusDrag};
		case EGizmoState::PositionMoveFocusDrag:
			return FGizmoContext{0, EGizmoAction::MoveFocusDrag};
		case EGizmoState::PositionLoseFocus:
			return FGizmoContext{0, EGizmoAction::LoseFocus};
		case EGizmoState::PositionVerifyFocusCommit:
			return FGizmoContext{0, EGizmoAction::VerifyFocusCommit};
		case EGizmoState::RotationPrepareModel:
			return FGizmoContext{1, EGizmoAction::PrepareModel};
		case EGizmoState::RotationPressMode:
			return FGizmoContext{1, EGizmoAction::PressMode};
		case EGizmoState::RotationReleaseMode:
			return FGizmoContext{1, EGizmoAction::ReleaseMode};
		case EGizmoState::RotationBeginDrag:
			return FGizmoContext{1, EGizmoAction::BeginDrag};
		case EGizmoState::RotationMoveDrag:
			return FGizmoContext{1, EGizmoAction::MoveDrag};
		case EGizmoState::RotationVerifyPreviewAndRelease:
			return FGizmoContext{1, EGizmoAction::VerifyPreviewAndRelease};
		case EGizmoState::RotationVerifyHistory:
			return FGizmoContext{1, EGizmoAction::VerifyHistory};
		case EGizmoState::RotationBeginCancelledDrag:
			return FGizmoContext{1, EGizmoAction::BeginCancelledDrag};
		case EGizmoState::RotationMoveCancelledDrag:
			return FGizmoContext{1, EGizmoAction::MoveCancelledDrag};
		case EGizmoState::RotationPressEscape:
			return FGizmoContext{1, EGizmoAction::PressEscape};
		case EGizmoState::RotationReleaseEscape:
			return FGizmoContext{1, EGizmoAction::ReleaseEscape};
		case EGizmoState::RotationVerifyCancellation:
			return FGizmoContext{1, EGizmoAction::VerifyCancellation};
		case EGizmoState::RotationBeginFocusDrag:
			return FGizmoContext{1, EGizmoAction::BeginFocusDrag};
		case EGizmoState::RotationMoveFocusDrag:
			return FGizmoContext{1, EGizmoAction::MoveFocusDrag};
		case EGizmoState::RotationLoseFocus:
			return FGizmoContext{1, EGizmoAction::LoseFocus};
		case EGizmoState::RotationVerifyFocusCommit:
			return FGizmoContext{1, EGizmoAction::VerifyFocusCommit};
		case EGizmoState::ScalePrepareModel:
			return FGizmoContext{2, EGizmoAction::PrepareModel};
		case EGizmoState::ScalePressMode:
			return FGizmoContext{2, EGizmoAction::PressMode};
		case EGizmoState::ScaleReleaseMode:
			return FGizmoContext{2, EGizmoAction::ReleaseMode};
		case EGizmoState::ScaleBeginDrag:
			return FGizmoContext{2, EGizmoAction::BeginDrag};
		case EGizmoState::ScaleMoveDrag:
			return FGizmoContext{2, EGizmoAction::MoveDrag};
		case EGizmoState::ScaleVerifyPreviewAndRelease:
			return FGizmoContext{2, EGizmoAction::VerifyPreviewAndRelease};
		case EGizmoState::ScaleVerifyHistory:
			return FGizmoContext{2, EGizmoAction::VerifyHistory};
		case EGizmoState::ScaleBeginCancelledDrag:
			return FGizmoContext{2, EGizmoAction::BeginCancelledDrag};
		case EGizmoState::ScaleMoveCancelledDrag:
			return FGizmoContext{2, EGizmoAction::MoveCancelledDrag};
		case EGizmoState::ScalePressEscape:
			return FGizmoContext{2, EGizmoAction::PressEscape};
		case EGizmoState::ScaleReleaseEscape:
			return FGizmoContext{2, EGizmoAction::ReleaseEscape};
		case EGizmoState::ScaleVerifyCancellation:
			return FGizmoContext{2, EGizmoAction::VerifyCancellation};
		case EGizmoState::ScaleBeginFocusDrag:
			return FGizmoContext{2, EGizmoAction::BeginFocusDrag};
		case EGizmoState::ScaleMoveFocusDrag:
			return FGizmoContext{2, EGizmoAction::MoveFocusDrag};
		case EGizmoState::ScaleLoseFocus:
			return FGizmoContext{2, EGizmoAction::LoseFocus};
		case EGizmoState::ScaleVerifyFocusCommit:
			return FGizmoContext{2, EGizmoAction::VerifyFocusCommit};
		default:
			return std::nullopt;
	}
}

inline bool IsGizmoState(EGizmoState InState)
{
	return FindGizmoContext(InState).has_value();
}

inline FGizmoContext DescribeGizmoContext(EGizmoState InState)
{
	const auto Context = FindGizmoContext(InState);
	if (!Context)
	{
		throw std::logic_error("Invalid Gizmo acceptance context");
	}
	return *Context;
}

enum class EMultiDetailsAction
{
	PreparePass,
	PressField,
	ReleaseField,
	PressSelectAll,
	ReleaseSelectAllAndType,
	SubmitValue,
	FinishSubmission,
	VerifyHistory,
};

struct FMultiDetailsContext
{
	unsigned CaseIndex{};
	EMultiDetailsAction Action{};
};

inline std::optional<FMultiDetailsContext> FindMultiDetailsContext(EMultiSelectionState InState)
{
	switch (InState)
	{
		case EMultiSelectionState::InitialPreparePass:
			return FMultiDetailsContext{0, EMultiDetailsAction::PreparePass};
		case EMultiSelectionState::InitialPressField:
			return FMultiDetailsContext{0, EMultiDetailsAction::PressField};
		case EMultiSelectionState::InitialReleaseField:
			return FMultiDetailsContext{0, EMultiDetailsAction::ReleaseField};
		case EMultiSelectionState::InitialPressSelectAll:
			return FMultiDetailsContext{0, EMultiDetailsAction::PressSelectAll};
		case EMultiSelectionState::InitialReleaseSelectAllAndType:
			return FMultiDetailsContext{0, EMultiDetailsAction::ReleaseSelectAllAndType};
		case EMultiSelectionState::InitialSubmitValue:
			return FMultiDetailsContext{0, EMultiDetailsAction::SubmitValue};
		case EMultiSelectionState::InitialFinishSubmission:
			return FMultiDetailsContext{0, EMultiDetailsAction::FinishSubmission};
		case EMultiSelectionState::InitialVerifyHistory:
			return FMultiDetailsContext{0, EMultiDetailsAction::VerifyHistory};
		case EMultiSelectionState::UnchangedPrimaryPreparePass:
			return FMultiDetailsContext{1, EMultiDetailsAction::PreparePass};
		case EMultiSelectionState::UnchangedPrimaryPressField:
			return FMultiDetailsContext{1, EMultiDetailsAction::PressField};
		case EMultiSelectionState::UnchangedPrimaryReleaseField:
			return FMultiDetailsContext{1, EMultiDetailsAction::ReleaseField};
		case EMultiSelectionState::UnchangedPrimaryPressSelectAll:
			return FMultiDetailsContext{1, EMultiDetailsAction::PressSelectAll};
		case EMultiSelectionState::UnchangedPrimaryReleaseSelectAllAndType:
			return FMultiDetailsContext{1, EMultiDetailsAction::ReleaseSelectAllAndType};
		case EMultiSelectionState::UnchangedPrimarySubmitValue:
			return FMultiDetailsContext{1, EMultiDetailsAction::SubmitValue};
		case EMultiSelectionState::UnchangedPrimaryFinishSubmission:
			return FMultiDetailsContext{1, EMultiDetailsAction::FinishSubmission};
		case EMultiSelectionState::UnchangedPrimaryVerifyHistory:
			return FMultiDetailsContext{1, EMultiDetailsAction::VerifyHistory};
		default:
			return std::nullopt;
	}
}

inline bool IsMultiDetailsState(EMultiSelectionState InState)
{
	return FindMultiDetailsContext(InState).has_value();
}

inline FMultiDetailsContext DescribeMultiDetailsContext(EMultiSelectionState InState)
{
	const auto Context = FindMultiDetailsContext(InState);
	if (!Context)
	{
		throw std::logic_error("Invalid MultiDetails acceptance context");
	}
	return *Context;
}

enum class EMultiGizmoAction
{
	PrepareModels,
	BeginDrag,
	MoveDrag,
	ReleaseDrag,
	VerifyCommit,
	VerifyUndo,
	Redo,
	VerifyRedo,
};

struct FMultiGizmoContext
{
	unsigned CaseIndex{};
	EMultiGizmoAction Action{};
};

inline std::optional<FMultiGizmoContext> FindMultiGizmoContext(EMultiSelectionState InState)
{
	switch (InState)
	{
		case EMultiSelectionState::PositionPrepareModels:
			return FMultiGizmoContext{0, EMultiGizmoAction::PrepareModels};
		case EMultiSelectionState::PositionBeginDrag:
			return FMultiGizmoContext{0, EMultiGizmoAction::BeginDrag};
		case EMultiSelectionState::PositionMoveDrag:
			return FMultiGizmoContext{0, EMultiGizmoAction::MoveDrag};
		case EMultiSelectionState::PositionReleaseDrag:
			return FMultiGizmoContext{0, EMultiGizmoAction::ReleaseDrag};
		case EMultiSelectionState::PositionVerifyCommit:
			return FMultiGizmoContext{0, EMultiGizmoAction::VerifyCommit};
		case EMultiSelectionState::PositionVerifyUndo:
			return FMultiGizmoContext{0, EMultiGizmoAction::VerifyUndo};
		case EMultiSelectionState::PositionRedo:
			return FMultiGizmoContext{0, EMultiGizmoAction::Redo};
		case EMultiSelectionState::PositionVerifyRedo:
			return FMultiGizmoContext{0, EMultiGizmoAction::VerifyRedo};
		case EMultiSelectionState::RotationPrepareModels:
			return FMultiGizmoContext{1, EMultiGizmoAction::PrepareModels};
		case EMultiSelectionState::RotationBeginDrag:
			return FMultiGizmoContext{1, EMultiGizmoAction::BeginDrag};
		case EMultiSelectionState::RotationMoveDrag:
			return FMultiGizmoContext{1, EMultiGizmoAction::MoveDrag};
		case EMultiSelectionState::RotationReleaseDrag:
			return FMultiGizmoContext{1, EMultiGizmoAction::ReleaseDrag};
		case EMultiSelectionState::RotationVerifyCommit:
			return FMultiGizmoContext{1, EMultiGizmoAction::VerifyCommit};
		case EMultiSelectionState::RotationVerifyUndo:
			return FMultiGizmoContext{1, EMultiGizmoAction::VerifyUndo};
		case EMultiSelectionState::RotationRedo:
			return FMultiGizmoContext{1, EMultiGizmoAction::Redo};
		case EMultiSelectionState::RotationVerifyRedo:
			return FMultiGizmoContext{1, EMultiGizmoAction::VerifyRedo};
		case EMultiSelectionState::ScalePrepareModels:
			return FMultiGizmoContext{2, EMultiGizmoAction::PrepareModels};
		case EMultiSelectionState::ScaleBeginDrag:
			return FMultiGizmoContext{2, EMultiGizmoAction::BeginDrag};
		case EMultiSelectionState::ScaleMoveDrag:
			return FMultiGizmoContext{2, EMultiGizmoAction::MoveDrag};
		case EMultiSelectionState::ScaleReleaseDrag:
			return FMultiGizmoContext{2, EMultiGizmoAction::ReleaseDrag};
		case EMultiSelectionState::ScaleVerifyCommit:
			return FMultiGizmoContext{2, EMultiGizmoAction::VerifyCommit};
		case EMultiSelectionState::ScaleVerifyUndo:
			return FMultiGizmoContext{2, EMultiGizmoAction::VerifyUndo};
		case EMultiSelectionState::ScaleRedo:
			return FMultiGizmoContext{2, EMultiGizmoAction::Redo};
		case EMultiSelectionState::ScaleVerifyRedo:
			return FMultiGizmoContext{2, EMultiGizmoAction::VerifyRedo};
		default:
			return std::nullopt;
	}
}

inline bool IsMultiGizmoState(EMultiSelectionState InState)
{
	return FindMultiGizmoContext(InState).has_value();
}

inline FMultiGizmoContext DescribeMultiGizmoContext(EMultiSelectionState InState)
{
	const auto Context = FindMultiGizmoContext(InState);
	if (!Context)
	{
		throw std::logic_error("Invalid MultiGizmo acceptance context");
	}
	return *Context;
}

enum class EReparentSelectionAction
{
	PrepareSelection,
	PressViewport,
	ReleaseViewport,
	VerifyViewportSelection,
	PressViewportDrag,
	MoveViewportDrag,
	MoveToOutliner,
	CancelViewportDrag,
	ReleaseViewportDrag,
	VerifyCancelledDrag,
	PressOutliner,
	StartOutlinerDrag,
	MoveOutlinerTarget,
	ReleaseOutlinerDrag,
	VerifyReparent,
};

enum class EReparentSelectionSubject
{
	Mesh,
	Light,
};

struct FReparentSelectionContext
{
	EReparentSelectionSubject Subject{};
	EReparentSelectionAction Action{};
};

inline std::optional<FReparentSelectionContext> FindReparentSelectionContext(EReparentSelectionState InState)
{
	switch (InState)
	{
		case EReparentSelectionState::MeshPrepareSelection:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::PrepareSelection};
		case EReparentSelectionState::MeshPressViewport:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh, EReparentSelectionAction::PressViewport};
		case EReparentSelectionState::MeshReleaseViewport:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::ReleaseViewport};
		case EReparentSelectionState::MeshVerifyViewportSelection:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::VerifyViewportSelection};
		case EReparentSelectionState::MeshPressViewportDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::PressViewportDrag};
		case EReparentSelectionState::MeshMoveViewportDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::MoveViewportDrag};
		case EReparentSelectionState::MeshMoveToOutliner:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh, EReparentSelectionAction::MoveToOutliner};
		case EReparentSelectionState::MeshCancelViewportDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::CancelViewportDrag};
		case EReparentSelectionState::MeshReleaseViewportDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::ReleaseViewportDrag};
		case EReparentSelectionState::MeshVerifyCancelledDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::VerifyCancelledDrag};
		case EReparentSelectionState::MeshPressOutliner:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh, EReparentSelectionAction::PressOutliner};
		case EReparentSelectionState::MeshStartOutlinerDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::StartOutlinerDrag};
		case EReparentSelectionState::MeshMoveOutlinerTarget:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::MoveOutlinerTarget};
		case EReparentSelectionState::MeshReleaseOutlinerDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh,
			                                 EReparentSelectionAction::ReleaseOutlinerDrag};
		case EReparentSelectionState::MeshVerifyReparent:
			return FReparentSelectionContext{EReparentSelectionSubject::Mesh, EReparentSelectionAction::VerifyReparent};
		case EReparentSelectionState::LightPrepareSelection:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::PrepareSelection};
		case EReparentSelectionState::LightPressViewport:
			return FReparentSelectionContext{EReparentSelectionSubject::Light, EReparentSelectionAction::PressViewport};
		case EReparentSelectionState::LightReleaseViewport:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::ReleaseViewport};
		case EReparentSelectionState::LightVerifyViewportSelection:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::VerifyViewportSelection};
		case EReparentSelectionState::LightPressViewportDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::PressViewportDrag};
		case EReparentSelectionState::LightMoveViewportDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::MoveViewportDrag};
		case EReparentSelectionState::LightMoveToOutliner:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::MoveToOutliner};
		case EReparentSelectionState::LightCancelViewportDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::CancelViewportDrag};
		case EReparentSelectionState::LightReleaseViewportDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::ReleaseViewportDrag};
		case EReparentSelectionState::LightVerifyCancelledDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::VerifyCancelledDrag};
		case EReparentSelectionState::LightPressOutliner:
			return FReparentSelectionContext{EReparentSelectionSubject::Light, EReparentSelectionAction::PressOutliner};
		case EReparentSelectionState::LightStartOutlinerDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::StartOutlinerDrag};
		case EReparentSelectionState::LightMoveOutlinerTarget:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::MoveOutlinerTarget};
		case EReparentSelectionState::LightReleaseOutlinerDrag:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::ReleaseOutlinerDrag};
		case EReparentSelectionState::LightVerifyReparent:
			return FReparentSelectionContext{EReparentSelectionSubject::Light,
			                                 EReparentSelectionAction::VerifyReparent};
		default:
			return std::nullopt;
	}
}

inline bool IsReparentSelectionState(EReparentSelectionState InState)
{
	return FindReparentSelectionContext(InState).has_value();
}

inline FReparentSelectionContext DescribeReparentSelectionContext(EReparentSelectionState InState)
{
	const auto Context = FindReparentSelectionContext(InState);
	if (!Context)
	{
		throw std::logic_error("Invalid ReparentSelection acceptance context");
	}
	return *Context;
}

enum class ELightPriorityAction
{
	PrepareLight,
	FocusPriority,
	PressSelectAll,
	ReleaseSelectAllAndType,
	PressEnter,
	ReleaseEnter,
	VerifyPriority,
	VerifyHistory,
};

struct FLightPriorityContext
{
	unsigned CaseIndex{};
	ELightPriorityAction Action{};
};

inline std::optional<FLightPriorityContext> FindLightPriorityContext(ELightPriorityState InState)
{
	switch (InState)
	{
		case ELightPriorityState::DirectionalPrepareLight:
			return FLightPriorityContext{0, ELightPriorityAction::PrepareLight};
		case ELightPriorityState::DirectionalFocusPriority:
			return FLightPriorityContext{0, ELightPriorityAction::FocusPriority};
		case ELightPriorityState::DirectionalPressSelectAll:
			return FLightPriorityContext{0, ELightPriorityAction::PressSelectAll};
		case ELightPriorityState::DirectionalReleaseSelectAllAndType:
			return FLightPriorityContext{0, ELightPriorityAction::ReleaseSelectAllAndType};
		case ELightPriorityState::DirectionalPressEnter:
			return FLightPriorityContext{0, ELightPriorityAction::PressEnter};
		case ELightPriorityState::DirectionalReleaseEnter:
			return FLightPriorityContext{0, ELightPriorityAction::ReleaseEnter};
		case ELightPriorityState::DirectionalVerifyPriority:
			return FLightPriorityContext{0, ELightPriorityAction::VerifyPriority};
		case ELightPriorityState::DirectionalVerifyHistory:
			return FLightPriorityContext{0, ELightPriorityAction::VerifyHistory};
		case ELightPriorityState::EnvironmentPrepareLight:
			return FLightPriorityContext{1, ELightPriorityAction::PrepareLight};
		case ELightPriorityState::EnvironmentFocusPriority:
			return FLightPriorityContext{1, ELightPriorityAction::FocusPriority};
		case ELightPriorityState::EnvironmentPressSelectAll:
			return FLightPriorityContext{1, ELightPriorityAction::PressSelectAll};
		case ELightPriorityState::EnvironmentReleaseSelectAllAndType:
			return FLightPriorityContext{1, ELightPriorityAction::ReleaseSelectAllAndType};
		case ELightPriorityState::EnvironmentPressEnter:
			return FLightPriorityContext{1, ELightPriorityAction::PressEnter};
		case ELightPriorityState::EnvironmentReleaseEnter:
			return FLightPriorityContext{1, ELightPriorityAction::ReleaseEnter};
		case ELightPriorityState::EnvironmentVerifyPriority:
			return FLightPriorityContext{1, ELightPriorityAction::VerifyPriority};
		case ELightPriorityState::EnvironmentVerifyHistory:
			return FLightPriorityContext{1, ELightPriorityAction::VerifyHistory};
		default:
			return std::nullopt;
	}
}

inline bool IsLightPriorityState(ELightPriorityState InState)
{
	return FindLightPriorityContext(InState).has_value();
}

inline FLightPriorityContext DescribeLightPriorityContext(ELightPriorityState InState)
{
	const auto Context = FindLightPriorityContext(InState);
	if (!Context)
	{
		throw std::logic_error("Invalid LightPriority acceptance context");
	}
	return *Context;
}

enum class EShortcutRangeAction
{
	PressRow,
	ReleaseRow,
	VerifyRange,
};

struct FShortcutRangeContext
{
	unsigned CaseIndex{};
	EShortcutRangeAction Action{};
};

inline std::optional<FShortcutRangeContext> FindShortcutRangeContext(EShortcutState InState)
{
	switch (InState)
	{
		case EShortcutState::PlainPressRow:
			return FShortcutRangeContext{0, EShortcutRangeAction::PressRow};
		case EShortcutState::PlainReleaseRow:
			return FShortcutRangeContext{0, EShortcutRangeAction::ReleaseRow};
		case EShortcutState::PlainVerifyRange:
			return FShortcutRangeContext{0, EShortcutRangeAction::VerifyRange};
		case EShortcutState::ExtendForwardPressRow:
			return FShortcutRangeContext{1, EShortcutRangeAction::PressRow};
		case EShortcutState::ExtendForwardReleaseRow:
			return FShortcutRangeContext{1, EShortcutRangeAction::ReleaseRow};
		case EShortcutState::ExtendForwardVerifyRange:
			return FShortcutRangeContext{1, EShortcutRangeAction::VerifyRange};
		case EShortcutState::ExtendBackwardPressRow:
			return FShortcutRangeContext{2, EShortcutRangeAction::PressRow};
		case EShortcutState::ExtendBackwardReleaseRow:
			return FShortcutRangeContext{2, EShortcutRangeAction::ReleaseRow};
		case EShortcutState::ExtendBackwardVerifyRange:
			return FShortcutRangeContext{2, EShortcutRangeAction::VerifyRange};
		case EShortcutState::AddPressRow:
			return FShortcutRangeContext{3, EShortcutRangeAction::PressRow};
		case EShortcutState::AddReleaseRow:
			return FShortcutRangeContext{3, EShortcutRangeAction::ReleaseRow};
		case EShortcutState::AddVerifyRange:
			return FShortcutRangeContext{3, EShortcutRangeAction::VerifyRange};
		case EShortcutState::ReplaceRangePressRow:
			return FShortcutRangeContext{4, EShortcutRangeAction::PressRow};
		case EShortcutState::ReplaceRangeReleaseRow:
			return FShortcutRangeContext{4, EShortcutRangeAction::ReleaseRow};
		case EShortcutState::ReplaceRangeVerifyRange:
			return FShortcutRangeContext{4, EShortcutRangeAction::VerifyRange};
		case EShortcutState::AddRangePressRow:
			return FShortcutRangeContext{5, EShortcutRangeAction::PressRow};
		case EShortcutState::AddRangeReleaseRow:
			return FShortcutRangeContext{5, EShortcutRangeAction::ReleaseRow};
		case EShortcutState::AddRangeVerifyRange:
			return FShortcutRangeContext{5, EShortcutRangeAction::VerifyRange};
		case EShortcutState::FilteredRangePressRow:
			return FShortcutRangeContext{6, EShortcutRangeAction::PressRow};
		case EShortcutState::FilteredRangeReleaseRow:
			return FShortcutRangeContext{6, EShortcutRangeAction::ReleaseRow};
		case EShortcutState::FilteredRangeVerifyRange:
			return FShortcutRangeContext{6, EShortcutRangeAction::VerifyRange};
		default:
			return std::nullopt;
	}
}

inline bool IsShortcutRangeState(EShortcutState InState)
{
	return FindShortcutRangeContext(InState).has_value();
}

inline FShortcutRangeContext DescribeShortcutRangeContext(EShortcutState InState)
{
	const auto Context = FindShortcutRangeContext(InState);
	if (!Context)
	{
		throw std::logic_error("Invalid ShortcutRange acceptance context");
	}
	return *Context;
}

enum class ETransformTextAction
{
	FocusField,
	SelectAllPress,
	TypeValue,
	ConfirmPress,
	ConfirmRelease,
};

struct FTransformTextContext
{
	unsigned CaseIndex{};
	ETransformTextAction Action{};
};

inline std::optional<FTransformTextContext> FindTransformTextContext(ETransformState InState)
{
	switch (InState)
	{
		case ETransformState::PositionFocusField:
			return FTransformTextContext{0, ETransformTextAction::FocusField};
		case ETransformState::PositionSelectAllPress:
			return FTransformTextContext{0, ETransformTextAction::SelectAllPress};
		case ETransformState::PositionTypeValue:
			return FTransformTextContext{0, ETransformTextAction::TypeValue};
		case ETransformState::PositionConfirmPress:
			return FTransformTextContext{0, ETransformTextAction::ConfirmPress};
		case ETransformState::PositionConfirmRelease:
			return FTransformTextContext{0, ETransformTextAction::ConfirmRelease};
		case ETransformState::RotationFocusField:
			return FTransformTextContext{1, ETransformTextAction::FocusField};
		case ETransformState::RotationSelectAllPress:
			return FTransformTextContext{1, ETransformTextAction::SelectAllPress};
		case ETransformState::RotationTypeValue:
			return FTransformTextContext{1, ETransformTextAction::TypeValue};
		case ETransformState::RotationConfirmPress:
			return FTransformTextContext{1, ETransformTextAction::ConfirmPress};
		case ETransformState::RotationConfirmRelease:
			return FTransformTextContext{1, ETransformTextAction::ConfirmRelease};
		case ETransformState::ScaleFocusField:
			return FTransformTextContext{2, ETransformTextAction::FocusField};
		case ETransformState::ScaleSelectAllPress:
			return FTransformTextContext{2, ETransformTextAction::SelectAllPress};
		case ETransformState::ScaleTypeValue:
			return FTransformTextContext{2, ETransformTextAction::TypeValue};
		case ETransformState::ScaleConfirmPress:
			return FTransformTextContext{2, ETransformTextAction::ConfirmPress};
		case ETransformState::ScaleConfirmRelease:
			return FTransformTextContext{2, ETransformTextAction::ConfirmRelease};
		default:
			return std::nullopt;
	}
}

inline bool IsTransformTextState(ETransformState InState)
{
	return FindTransformTextContext(InState).has_value();
}

inline FTransformTextContext DescribeTransformTextContext(ETransformState InState)
{
	const auto Context = FindTransformTextContext(InState);
	if (!Context)
	{
		throw std::logic_error("Invalid TransformText acceptance context");
	}
	return *Context;
}

enum class ETransformDragAction
{
	BeginDrag,
	MoveForward,
	MoveBackward,
	ReleaseDrag,
	VerifyDrag,
	PressUndo,
	ReleaseUndo,
	VerifyUndo,
};

struct FTransformDragContext
{
	unsigned CaseIndex{};
	ETransformDragAction Action{};
};

inline std::optional<FTransformDragContext> FindTransformDragContext(ETransformState InState)
{
	switch (InState)
	{
		case ETransformState::BeginNumericDrag:
			return FTransformDragContext{0, ETransformDragAction::BeginDrag};
		case ETransformState::MoveNumericDragForward:
			return FTransformDragContext{0, ETransformDragAction::MoveForward};
		case ETransformState::MoveNumericDragBackward:
			return FTransformDragContext{0, ETransformDragAction::MoveBackward};
		case ETransformState::ReleaseNumericDrag:
			return FTransformDragContext{0, ETransformDragAction::ReleaseDrag};
		case ETransformState::VerifyNumericDrag:
			return FTransformDragContext{0, ETransformDragAction::VerifyDrag};
		case ETransformState::UndoNumericDragPress:
			return FTransformDragContext{0, ETransformDragAction::PressUndo};
		case ETransformState::UndoNumericDragRelease:
			return FTransformDragContext{0, ETransformDragAction::ReleaseUndo};
		case ETransformState::VerifyNumericDragUndo:
			return FTransformDragContext{0, ETransformDragAction::VerifyUndo};
		default:
			return std::nullopt;
	}
}

inline bool IsTransformDragState(ETransformState InState)
{
	return FindTransformDragContext(InState).has_value();
}

inline FTransformDragContext DescribeTransformDragContext(ETransformState InState)
{
	const auto Context = FindTransformDragContext(InState);
	if (!Context)
	{
		throw std::logic_error("Invalid TransformDrag acceptance context");
	}
	return *Context;
}

} // namespace Hyperion
