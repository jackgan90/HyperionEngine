#pragma once
#include "Hyperion/Renderer/RenderSession.h"
#include <atomic>
#include <map>
#include <mutex>
#include <set>

namespace Hyperion
{
struct FRenderSession::FMaterialState
{
	struct FViewEntry
	{
		FMaterialScopeInput Scope;
		FMaterialParameterValues Values;
	};

	std::uint64_t Identity{};
	std::atomic_uint64_t NextFrame{1};
	ERHIDepthFormat Depth;
	FMaterialProviderRegistry Providers;
	std::mutex Publication;
	FMaterialProviderInputs Inputs;
	std::map<std::uint64_t, FViewEntry> Views;
	std::uint64_t LastFrame{};
	std::set<std::uint64_t> Families;

	FMaterialState(ERHIDepthFormat InDepth, std::shared_ptr<const FMaterialSemanticRegistry> InSemantics)
	    : Depth(InDepth), Providers(std::move(InSemantics))
	{
		static std::atomic_uint64_t NextIdentity{1};
		Identity = NextIdentity.fetch_add(1);
	}

	std::shared_ptr<const FMaterialFrameContext> Frame(const FRenderResourceService& InResources, float InTime,
	                                                   FMaterialParameterValues InValues,
	                                                   std::uint64_t InSceneIdentity);
};
} // namespace Hyperion
