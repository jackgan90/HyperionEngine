#pragma once
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderResources.h"

namespace Hyperion
{
// Per preparation/view snapshots; no shared counter updates in the item loop.
struct FMaterialPreparationProfile
{
#if HYP_ENABLE_PROFILING
	bool bEnabled = IsProfilingEnabled(EProfileCategory::Material);
	FMaterialProviderRegistry& Providers;
	FMaterialProviderStats Before;
	std::uint64_t Reuses{};
	std::uint64_t Refreshes{};
	std::uint64_t Full{};
	std::uint64_t SharedUpdates{};
	explicit FMaterialPreparationProfile(FMaterialProviderRegistry& InProviders);
	~FMaterialPreparationProfile();

	void Reused()
	{
		Reuses += bEnabled;
	}

	void Refreshed()
	{
		Refreshes += bEnabled;
	}

	void Evaluated()
	{
		Full += bEnabled;
	}

	void SharedUpdate()
	{
		SharedUpdates += bEnabled;
	}
#else
	explicit FMaterialPreparationProfile(FMaterialProviderRegistry&)
	{
	}

	void Reused()
	{
	}

	void Refreshed()
	{
	}

	void Evaluated()
	{
	}

	void SharedUpdate()
	{
	}
#endif
};

void PlotMaterialDrawCounters(const FRenderResourceStats& InBefore, const FRenderResourceStats& InAfter);
} // namespace Hyperion
