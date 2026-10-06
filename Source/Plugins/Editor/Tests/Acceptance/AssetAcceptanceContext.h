#pragma once
#include "AcceptanceTransition.h"

namespace Hyperion
{
enum class EEditorAcceptanceWindow
{
	Main,
	Asset
};

enum class EAssetAcceptanceStage
{
	Opening,
	Name,
	Saving,
	Material,
	References,
	Texture,
	CustomMaterial,
	CustomMaterialReset,
	CleanTab,
	DiscardTab,
	FloatingPanel,
	WindowFixture,
	WindowHistory,
	WindowSizing,
	WindowClosing,
	WindowSaving,
};

struct FAssetAcceptanceContext
{
	EAssetAcceptanceStage Stage;
	EEditorAcceptanceWindow InputWindow;
};

inline FAssetAcceptanceContext DescribeAssetAcceptance(EAssetState InState)
{
	switch (InState)
	{
#define HYP_ACCEPTANCE_ASSET_STATE(Name, Stage, Window)                                                                \
	case EAssetState::Name:                                                                                            \
		return {EAssetAcceptanceStage::Stage, EEditorAcceptanceWindow::Window};
#include "AssetAcceptanceStates.inl"
#undef HYP_ACCEPTANCE_ASSET_STATE
	}
	throw std::logic_error("Unknown asset acceptance state");
}
} // namespace Hyperion
