#pragma once
#include "Hyperion/Renderer/RenderSession.h"
#include <atomic>
#include <map>
#include <mutex>
#include <set>

namespace Hyperion
{
struct FLocalMaterialPreparation;

struct FRenderSession::FMaterialState
{
	struct FViewEntry
	{
		FMaterialScopeInput Scope;
		FMaterialInputValues Values;
		std::uint64_t AccessFrame{};
	};

	struct FPreparedView
	{
		std::shared_ptr<FRenderSceneSnapshot> Snapshot;
		std::shared_ptr<const FLocalMaterialPreparation> LocalPreparation;
		std::vector<FRenderTargetSource> GraphReads;
		std::uint64_t SceneRevision{};
		std::uint64_t ResourceRevision{};
		std::uint32_t Dependencies{};
		bool bValid{};
		bool bDepthSorted{};
		std::uint64_t AccessFrame{};
	};

	std::uint64_t Identity{};
	std::atomic_uint64_t NextFrame{1};
	ERHIDepthFormat Depth;
	FMaterialProviderRegistry Providers;
	std::vector<std::weak_ptr<const FMaterialSharedBinding>> SharedBindings;
	std::mutex Publication;
	bool bProvidersFrozen{}; // Guarded by Publication; freeze once before any frame reaches Render.
	FMaterialProviderInputs Inputs;
	std::map<std::uint64_t, FViewEntry> Views;
	std::map<std::uint64_t, FPreparedView> PreparedViews;
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
