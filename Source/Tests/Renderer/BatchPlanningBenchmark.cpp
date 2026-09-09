#include "Renderer/InstanceBatchSupport.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>

using namespace Hyperion;
using namespace Hyperion::InstanceTests;

namespace
{
struct FWorkload
{
	std::string_view Name;
	std::size_t Visible = 1024;
	std::size_t Step{};
	bool bReverse{};
	bool bValue{};
	bool bShared{};
	bool bState{};
};

FRenderSceneSnapshot Frame(const FRenderSceneSnapshot& InSource, const FWorkload& InWork, std::size_t InFrame)
{
	FRenderSceneSnapshot Result;
	Result.View = InSource.View;
	Result.DepthFormat = InSource.DepthFormat;
	const auto Schema = InSource.Items[0].State.Surface->GetCompiled()->Interface.Schema;
	std::shared_ptr<FMaterialSharedParameters> Shared;
	if (InWork.bShared)
	{
		Shared = std::make_shared<FMaterialSharedParameters>();
		std::vector<std::optional<std::shared_ptr<const FMaterialValue>>> Values(
		    InSource.Items[0].ResolvedParameters->Values.GetSize());
		Values[Schema->Find("SharedTint").Index] =
		    std::make_shared<const FMaterialValue>(FMaterialValue::Float(FVec4{float(InFrame % 137) / 137, 1, 1, 1}));
		Shared->Values = std::make_shared<const FMaterialValueTable::FSharedValues>(std::move(Values));
		auto Scopes = std::make_shared<FMaterialResolvedScopes::FValues>();
		for (std::size_t Index = 0; Index < MaterialScopeCount; ++Index)
		{
			(*Scopes)[Index] = InSource.Items[0].ResolvedParameters->Scopes[Index];
		}
		Shared->Scopes = std::move(Scopes);
	}
	for (std::size_t Offset = 0; Offset < InWork.Visible; ++Offset)
	{
		const auto Ordinal = InWork.bReverse && InFrame % 2 ? InWork.Visible - 1 - Offset : Offset;
		const auto Index = (InFrame * InWork.Step + Ordinal) % InSource.Items.Size();
		auto Item = InSource.Items[Index];
		Item.SharedParameters = Shared;
		if (InWork.bValue && Offset == 0)
		{
			auto Values = std::make_shared<FResolvedMaterialParameters>(*Item.ResolvedParameters);
			Values->Values.Set(
			    Schema->Find("Placement").Index,
			    std::make_shared<const FMaterialValue>(FMaterialValue::Float(FVec4{float(InFrame), 0, 0, 1})));
			Item.ResolvedParameters = std::move(Values);
		}
		if (InWork.bState && Index % 7 == InFrame % 7)
		{
			Item.DynamicState = FMaterialDynamicState{3};
		}
		Result.Items.PushBack(std::move(Item));
	}
	return Result;
}

void CheckCoverage(const FRenderSceneSnapshot& InFrame, const FRenderBatchPlan& InPlan)
{
	std::vector<unsigned> Coverage(InFrame.Items.Size());
	for (const auto& Batch : InPlan.Batches)
	{
		HYP_CHECK(!Batch.Items.empty());
		HYP_CHECK(Batch.Items.size() == 1 || (Batch.Instances && Batch.Instances->InstanceCount == Batch.Items.size()));
		for (const auto Index : Batch.Items)
		{
			HYP_CHECK(Index < Coverage.size());
			++Coverage[Index];
		}
	}
	HYP_CHECK(std::all_of(Coverage.begin(), Coverage.end(),
	                      [](auto InValue)
	                      {
		                      return InValue == 1;
	                      }));
}

void Run(FFixture& InFixture, const FRenderSceneSnapshot& InSource, const FWorkload& InWork, std::size_t InWarmup,
         std::size_t InSamples)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	std::vector<double> Times;
	FRenderBatchStats Statistics;
	std::size_t Draws{};
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    for (std::size_t Index = 0; Index < InWarmup + InSamples; ++Index)
		    {
			    const auto Input = Frame(InSource, InWork, Index);
			    const auto Start = std::chrono::steady_clock::now();
			    const auto Plan = Batches.Build(Input);
			    const auto Time =
			        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
			    CheckCoverage(Input, *Plan);
			    if (Index >= InWarmup)
			    {
				    if (InWork.Name == "stable" || InWork.Name == "shared")
				    {
					    HYP_CHECK(Plan->Statistics.PlanReuses == 1 && Plan->Statistics.RebuiltChunks == 0);
				    }
				    Times.push_back(Time);
				    Statistics += Plan->Statistics;
				    Draws += Plan->Batches.size();
			    }
		    }
	    }));
	std::sort(Times.begin(), Times.end());
	std::cout << InWork.Name << ',' << InWarmup << ',' << InSamples << ',' << InWork.Visible << ','
	          << std::accumulate(Times.begin(), Times.end(), 0.0) / Times.size() << ',' << Times[Times.size() / 2]
	          << ',' << Times[(Times.size() - 1) * 95 / 100] << ',' << Draws << ',' << Statistics.PlanReuses << ','
	          << Statistics.ReusedChunks << ',' << Statistics.RebuiltChunks << ',' << Statistics.PackedRecords << ','
	          << Statistics.PreparedInputBuilds << ',' << Statistics.PreparedInputReuses << ','
	          << Statistics.InstanceContractBuilds << '\n';
}
} // namespace

int main(int InCount, char** InArguments)
{
	try
	{
		const std::size_t Samples = InCount > 1 ? std::stoul(InArguments[1]) : 200;
		HYP_CHECK(Samples > 0 && Samples <= 10000);
		FFixture Fixture;
		const auto Source = Snapshot(Fixture, 1536);
		const std::array Workloads{FWorkload{"stable"},
		                           FWorkload{"shared", 1024, 0, false, false, true},
		                           FWorkload{"visibility-small", 1024, 1},
		                           FWorkload{"visibility-large", 1024, 37},
		                           FWorkload{"visibility-shared", 1024, 37, false, false, true},
		                           FWorkload{"reverse", 1024, 0, true},
		                           FWorkload{"one-value", 1024, 0, false, true},
		                           FWorkload{"state-split", 1024, 0, false, false, false, true}};
		std::cout << std::fixed << std::setprecision(6)
		          << "workload,warmup,samples,visible,mean_ms,median_ms,p95_ms,draws,plan_reuses,reused_chunks,rebuilt_"
		             "chunks,packed_records,prepared_builds,prepared_reuses,contract_builds\n";
		for (const auto& Work : Workloads)
		{
			Run(Fixture, Source, Work, 80, Samples);
		}
		HYP_CHECK(Fixture.Device->Statistics().ValidationErrors == 0);
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
