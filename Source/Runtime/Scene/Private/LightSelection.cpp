#include "Hyperion/Scene/LightSelection.h"
#include "Hyperion/Scene/Scene.h"

namespace Hyperion
{
FLightPrioritySelection ResolveLightPriority(std::span<const FLightPriorityCandidate> InCandidates)
{
	FLightPrioritySelection Result;
	const FLightPriorityCandidate* Best{};
	for (const auto& Candidate : InCandidates)
	{
		if (!Best || Candidate.Priority > Best->Priority)
		{
			Best = &Candidate;
			Result.bTied = false;
		}
		else if (Candidate.Priority == Best->Priority)
		{
			Result.bTied = true;
			if (Candidate.Id < Best->Id)
			{
				Best = &Candidate;
			}
		}
	}
	if (Best)
	{
		Result.Handle = Best->Handle;
	}
	return Result;
}

bool CanCastDirectionalShadows(const FSceneDirectionalLight& InLight)
{
	const auto Radiance = SceneLightRadiance(InLight.Color, InLight.Intensity);
	return InLight.bCastShadows && (Radiance.X > 0 || Radiance.Y > 0 || Radiance.Z > 0);
}

FSceneLightingSelection FScene::GetLightingSelection() const
{
	std::vector<FLightPriorityCandidate> Directional;
	std::vector<FLightPriorityCandidate> Environment;
	for (const auto Handle : GetNodes())
	{
		FSceneNodeView View;
		if (!GetNodeView(Handle, View) || !View.bEffectiveEnabled)
		{
			continue;
		}
		if (const auto& Light = View.Node->DirectionalLight(); Light && CanCastDirectionalShadows(*Light))
		{
			Directional.push_back({Handle, View.Node->Id, Light->Priority});
		}
		if (const auto& Light = View.Node->EnvironmentLight())
		{
			Environment.push_back({Handle, View.Node->Id, Light->Priority});
		}
	}
	return {ResolveLightPriority(Directional), ResolveLightPriority(Environment)};
}
} // namespace Hyperion
