#include "Hyperion/Renderer/SceneInstance.h"

namespace Hyperion
{
FSceneLightingInfo FSceneInstance::GetLightingInfo() const
{
	const auto Selection = GetLightingSelection();
	FSceneLightingInfo Result{GetRevision(), Selection.Directional.Handle, Selection.Environment.Handle, {}};
	const auto Append = [&](const FSceneNodeView& InView, const FLightPrioritySelection& InSelection,
	                        std::int32_t InPriority, bool bInEligible, ESceneLightDiagnosticKind InKind)
	{
		const bool bSky = InKind == ESceneLightDiagnosticKind::Sky;
		FSceneLightDiagnostic Entry;
		Entry.Handle = InView.Handle;
		Entry.Id = InView.Node->Id;
		Entry.Type = FSceneLightDiagnosticType(InKind);
		Entry.Priority = InPriority;
		Entry.bEnabled = InView.bEffectiveEnabled;
		Entry.bSelected = InSelection.Handle == InView.Handle;
		const auto* Winner = InSelection.Handle ? FindNode(*InSelection.Handle) : nullptr;
		const auto WinnerPriority = !Winner ? 0
		                            : bSky  ? Winner->EnvironmentLight()->Priority
		                                    : Winner->DirectionalLight()->Priority;
		Entry.bTied = InSelection.bTied && bInEligible && InPriority == WinnerPriority;
		Entry.Message = !InView.bEffectiveEnabled ? "Object or ancestor is disabled."
		                : !bInEligible            ? "Excluded from shadow selection: shadows disabled or zero radiance."
		                : Entry.bSelected
		                    ? (bSky ? "Current effective environment" : "Current directional shadow source")
		                    : std::string(bSky ? "Environment overridden by " : "Directional shadows overridden by ") +
		                          (Winner ? Winner->Name : std::string("none"));
		if (bSky && InView.Node->EnvironmentLight()->Source == ESceneEnvironmentSource::SkyAsset)
		{
			Entry.Asset = GetSkyAssetStatus(InView.Handle);
		}
		Result.Lights.push_back(std::move(Entry));
	};
	for (const auto Handle : GetNodes())
	{
		FSceneNodeView View;
		if (!GetNodeView(Handle, View))
		{
			continue;
		}
		if (const auto& Light = View.Node->DirectionalLight())
		{
			Append(View, Selection.Directional, Light->Priority,
			       View.bEffectiveEnabled && CanCastDirectionalShadows(*Light), ESceneLightDiagnosticKind::Directional);
		}
		if (const auto& Light = View.Node->EnvironmentLight())
		{
			Append(View, Selection.Environment, Light->Priority, View.bEffectiveEnabled,
			       ESceneLightDiagnosticKind::Sky);
		}
	}
	return Result;
}
} // namespace Hyperion
