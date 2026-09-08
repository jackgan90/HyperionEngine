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
		ProfilePlot("ProviderReuses", double(After.Reuses - Before.Reuses));
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
