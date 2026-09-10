#include "SessionMaterialsInternal.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
namespace
{
constexpr auto ScopeIndex(EMaterialScope InScope)
{
	return static_cast<std::size_t>(InScope);
}

void ReplaceValues(FMaterialProviderInputs& InInputs, EMaterialScope InScope, FMaterialParameterValues InValues,
                   const FRenderResourceService& InResources)
{
	FMaterialInputValues Values(std::move(InValues));
	auto& Scope = InInputs.Scopes[ScopeIndex(InScope)];
	if (Scope.Lifetime && InInputs.Values[ScopeIndex(InScope)] == Values)
	{
		return;
	}
	auto Lifetime = InResources.CreateScopeLifetime();
	InInputs.Values[ScopeIndex(InScope)] = std::move(Values);
	++Scope.Key.Revision;
	Scope.Lifetime = std::move(Lifetime);
}
} // namespace

std::shared_ptr<const FMaterialFrameContext> FRenderSession::FMaterialState::Frame(
    const FRenderResourceService& InResources, float InTime, FMaterialParameterValues InValues,
    std::uint64_t InSceneIdentity)
{
	if (!std::isfinite(InTime))
	{
		throw std::invalid_argument("Nonfinite material frame time");
	}
	std::lock_guard Lock(Publication);
	if (!bProvidersFrozen)
	{
		// Registry caches belong to Render. Do not rewrite its frozen flag from subsequent Main ticks.
		Providers.Freeze();
		bProvidersFrozen = true;
	}
	auto& SceneScope = Inputs.Scopes[ScopeIndex(EMaterialScope::Scene)];
	if (SceneScope.Key.GetQualifiers() != std::vector<std::uint64_t>{InSceneIdentity})
	{
		SceneScope.Key.SetQualifiers({InSceneIdentity});
		SceneScope.Lifetime = InResources.CreateScopeLifetime();
		++SceneScope.Key.Revision;
	}
	auto Result = std::make_shared<FMaterialFrameContext>();
	Result->Session = Identity;
	Result->Frame = NextFrame.fetch_add(1);
	Result->Inputs = Inputs;
	InValues.push_back({"Engine.Frame.Time", FMaterialValue::Float(InTime)});
	InValues.push_back({"Engine.Frame.Index", FMaterialValue::Uint(static_cast<std::uint32_t>(Result->Frame))});
	ReplaceValues(Result->Inputs, EMaterialScope::Frame, std::move(InValues), InResources);
	Result->Inputs.Scopes[ScopeIndex(EMaterialScope::Frame)].Key = {Identity, Result->Frame};
	return Result;
}

std::shared_ptr<const FMaterialFrameContext> FRenderSession::FreezeFrame(float InTime,
                                                                         FMaterialParameterValues InFrameValues)
{
	Tasks.Require({EDomain::Main});
	return MaterialState->Frame(Resources, InTime, std::move(InFrameValues), Scene.GetLogicalSceneIdentity());
}

void FRenderSession::SetGlobalParameters(FMaterialParameterValues InValues)
{
	Tasks.Require({EDomain::Main});
	std::lock_guard Lock(MaterialState->Publication);
	ReplaceValues(MaterialState->Inputs, EMaterialScope::Global, std::move(InValues), Resources);
}

void FRenderSession::SetSceneParameters(FMaterialParameterValues InValues)
{
	Tasks.Require({EDomain::Main});
	std::lock_guard Lock(MaterialState->Publication);
	ReplaceValues(MaterialState->Inputs, EMaterialScope::Scene, std::move(InValues), Resources);
}

FMaterialProviderRegistry& FRenderSession::GetProviders()
{
	Tasks.Require({EDomain::Main});
	return MaterialState->Providers;
}

FMaterialSharedValue FRenderSession::ResolveFrameSemantic(const FMaterialFrameContext& InFrame,
                                                          std::string_view InSemantic)
{
	Tasks.Require({EDomain::Render});
	if (bClosed || InFrame.Session != MaterialState->Identity || InFrame.Frame < MaterialState->LastFrame)
	{
		throw std::invalid_argument("Foreign or stale material frame");
	}
	FMaterialProviderInputs Inputs;
	for (const auto Scope : {EMaterialScope::Global, EMaterialScope::Frame, EMaterialScope::Scene})
	{
		Inputs.Scopes[ScopeIndex(Scope)] = InFrame.Inputs.Scopes[ScopeIndex(Scope)];
		Inputs.Values[ScopeIndex(Scope)] = InFrame.Inputs.Values[ScopeIndex(Scope)];
	}
	return MaterialState->Providers.EvaluateOne(Inputs, InSemantic).Value;
}
} // namespace Hyperion
