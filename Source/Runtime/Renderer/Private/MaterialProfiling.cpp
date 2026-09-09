#include "MaterialProfiling.h"
#include <numeric>

namespace Hyperion
{
#if HYP_ENABLE_PROFILING
FMaterialPreparationProfile::FMaterialPreparationProfile(FMaterialProviderRegistry& InProviders)
    : Providers(InProviders)
{
	if (bEnabled)
	{
		Before = Providers.Statistics();
	}
}

FMaterialPreparationProfile::~FMaterialPreparationProfile()
{
	if (bEnabled)
	{
		const auto After = Providers.Statistics();
		ProfilePlot("MaterialEvaluationReuses", double(Reuses));
		ProfilePlot("MaterialEvaluationRefreshes", double(Refreshes));
		ProfilePlot("MaterialEvaluationFull", double(Full));
		ProfilePlot("MaterialSharedUpdates", double(SharedUpdates));
		ProfilePlot("ProviderReuses", double(After.Reuses - Before.Reuses));
		ProfilePlot("ProviderCachedEntries", double(After.CachedEntries));
		ProfilePlot("ProviderCachedValueBytes", double(After.CachedValueBytes));
		ProfilePlot("ProviderEvictions", double(After.Evictions - Before.Evictions));
		ProfilePlot("ProviderEvaluations",
		            double(std::accumulate(After.Evaluations.begin(), After.Evaluations.end(), std::uint64_t{}) -
		                   std::accumulate(Before.Evaluations.begin(), Before.Evaluations.end(), std::uint64_t{})));
	}
}
#endif

void PlotMaterialDrawCounters(const FRenderResourceStats& InBefore, const FRenderResourceStats& InAfter)
{
	HYP_PERF_PLOT(Material, ConstantPacks, double(InAfter.Constants.Packs - InBefore.Constants.Packs));
	HYP_PERF_PLOT(Material, ConstantUploadBytes,
	              double(InAfter.Constants.UploadBytes - InBefore.Constants.UploadBytes));
	HYP_PERF_PLOT(Material, ConstantReuses, double(InAfter.Constants.Reuses - InBefore.Constants.Reuses));
	HYP_PERF_PLOT(Material, ConstantFullLookups,
	              double(InAfter.Constants.FullLookups - InBefore.Constants.FullLookups));
	HYP_PERF_PLOT(Material, ConstantPreparedReuses,
	              double(InAfter.Constants.PreparedReuses - InBefore.Constants.PreparedReuses));
	HYP_PERF_PLOT(Material, ConstantEvictions, double(InAfter.Constants.Evictions - InBefore.Constants.Evictions));
	HYP_PERF_PLOT(Material, ConstantCachedBlocks, double(InAfter.Constants.CachedBlocks));
	HYP_PERF_PLOT(Material, ConstantCachedBytes, double(InAfter.Constants.CachedBytes));
	HYP_PERF_PLOT(Material, ConstantPreparedBlocks, double(InAfter.Constants.PreparedBlocks));
	HYP_PERF_PLOT(Material, ConstantPageBytes, double(InAfter.Constants.PageBytes));
	HYP_PERF_PLOT(Material, BindingSetsCreated, double(InAfter.Materials.SetsCreated - InBefore.Materials.SetsCreated));
	HYP_PERF_PLOT(Material, BindingSetReuses, double(InAfter.Materials.SetReuses - InBefore.Materials.SetReuses));
	HYP_PERF_PLOT(Material, PipelinesCreated,
	              double(InAfter.Materials.PipelinesCreated - InBefore.Materials.PipelinesCreated));
	HYP_PERF_PLOT(Material, PipelineReuses,
	              double(InAfter.Materials.PipelineReuses - InBefore.Materials.PipelineReuses));
	(void)InBefore;
	(void)InAfter;
}
} // namespace Hyperion
