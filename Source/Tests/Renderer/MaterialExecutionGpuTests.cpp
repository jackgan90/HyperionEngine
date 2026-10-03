#include "Renderer/InstanceBatchSupport.h"

namespace Hyperion::InstanceTests
{
namespace
{
std::shared_ptr<const FRenderResource> RequestProgram(FFixture& InFixture, FMaterialDescription InDescription,
                                                      std::vector<FMaterialVariantRequest> InVariants)
{
	const auto Frozen = Surface(std::move(InDescription));
	const auto Program = std::make_shared<const FCompiledMaterialDefinition>(
	    CompileMaterialDefinition(InFixture.Compiler, Frozen->Definition, EShaderFormat::Dxil, std::move(InVariants)));
	const auto Resource =
	    InFixture.Session->GetResources().Request(std::make_shared<int>(0), 1, "explicit material execution",
	                                              [Frozen, Program]
	                                              {
		                                              auto Result = Geometry(Frozen);
		                                              Result.Materials.front().Compiled = Program;
		                                              return Result;
	                                              });
	WaitFor(
	    [&]
	    {
		    return Resource->GetStatus() == ERenderResourceStatus::Ready ||
		           Resource->GetStatus() == ERenderResourceStatus::Failed;
	    });
	HYP_CHECK(Resource->GetStatus() == ERenderResourceStatus::Ready);
	const auto Material = Resource->GetMaterial(0);
	WaitFor(
	    [&]
	    {
		    return Material->GetStatus() == ERenderMaterialStatus::Ready ||
		           Material->GetStatus() == ERenderMaterialStatus::Failed;
	    });
	HYP_CHECK(Material->GetStatus() == ERenderMaterialStatus::Ready && Material->GetCompiled() == Program);
	return Resource;
}

FRenderSceneSnapshot SelectProgram(FFixture& InFixture, const std::shared_ptr<const FRenderResource>& InResource)
{
	auto Result = Snapshot(InFixture, 4);
	for (auto& Item : Result.Items)
	{
		Item.State.Resource = InResource;
		Item.State.Surface = InResource->GetMaterial(0);
		const auto Program = Item.State.Surface->GetCompiled();
		Item.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(ResolveMaterialBindingContext(
		    Item.State.Surface->GetSnapshot(), *Program, Program->GetPass(), Item.Context));
	}
	return Result;
}

FRenderBatchStats BatchStatistics(FFixture& InFixture)
{
	FRenderBatchStats Result;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Rhi, 0},
	                                              [&]
	                                              {
		                                              Result = InFixture.Session->GetResources().Statistics().Batches;
	                                              }));
	return Result;
}

void ClearBatches(FFixture& InFixture, FRenderBatchSystem& InBatches, FRenderBatchSystem& InOrdinary)
{
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              InBatches.Clear();
		                                              InOrdinary.Clear();
	                                              }));
}

void CheckCustomProgram(FFixture& InFixture)
{
	const auto Resource =
	    RequestProgram(InFixture, Description(),
	                   {{"Forward", "Default"}, {"Forward", "Crowd", {}, EMaterialExecutionMode::Instanced}});
	const auto Items = SelectProgram(InFixture, Resource);
	const auto BaselineItems = Snapshot(InFixture, 4);
	const std::array<std::size_t, 4> Indices{0, 1, 2, 3};
	const auto CustomData = PackInstanceBatch(Items, Indices);
	const auto DefaultData = PackInstanceBatch(BaselineItems, Indices);
	HYP_CHECK(CustomData->Constants.size() == DefaultData->Constants.size());
	for (std::size_t Index = 0; Index < CustomData->Constants.size(); ++Index)
	{
		HYP_CHECK(*CustomData->Constants[Index].Bytes == *DefaultData->Constants[Index].Bytes);
	}
	auto Caps = InFixture.Device->GetCapabilities();
	FRenderBatchSystem Batches(InFixture.Tasks, Caps);
	Caps.Features[static_cast<std::size_t>(ERHIFeature::InstancedDrawing)].bEnabled = false;
	FRenderBatchSystem Ordinary(InFixture.Tasks, Caps);
	const auto Singles = Prepare(InFixture, Ordinary, Items);
	const auto Pixels = InFixture.Draw(Singles);
	const auto Batched = Prepare(InFixture, Batches, Items);
	HYP_CHECK(DrawCount(Singles) == 4 && DrawCount(Batched) == 1 && InstanceCount(Batched) == 4);
	HYP_CHECK(InFixture.Draw(Batched).Rgba == Pixels.Rgba);
	const auto Reused = Prepare(InFixture, Batches, Items);
	const auto Stats = BatchStatistics(InFixture);
	HYP_CHECK(Stats.ReusedChunks == 1 && Stats.UploadBytes == 0 && Stats.GpuReuses > 0);
	HYP_CHECK(InFixture.Draw(Reused).Rgba == Pixels.Rgba);
	const auto Other = Prepare(InFixture, Batches, BaselineItems);
	HYP_CHECK(InFixture.Draw(Other).Rgba == Pixels.Rgba);
	HYP_CHECK(InFixture.Draw(Batched).Rgba == Pixels.Rgba);
	ClearBatches(InFixture, Batches, Ordinary);
}

void CheckCustomNativeFallback(FFixture& InFixture)
{
	auto Desc = Description();
	Desc.Passes.front().Vertex = {"InstanceLimits.hlsl", "VSMain", {{"HYP_TEST_INSTANCE_INPUT", "1"}}};
	Desc.Passes.front().Pixel = {"InstanceLimits.hlsl", "PSMain", {{"HYP_TEST_INSTANCE_INPUT", "1"}}};
	const auto Resource =
	    RequestProgram(InFixture, std::move(Desc),
	                   {{"Forward", "Default"}, {"Forward", "Crowd", {}, EMaterialExecutionMode::Instanced}});
	HYP_CHECK(Resource->GetMaterial(0)->GetCompiled()->GetInstancePass().Variant == "Crowd");
	const auto Items = SelectProgram(InFixture, Resource);
	auto Caps = InFixture.Device->GetCapabilities();
	FRenderBatchSystem Batches(InFixture.Tasks, Caps);
	const auto Fallback = Prepare(InFixture, Batches, Items);
	const auto Stats = BatchStatistics(InFixture);
	HYP_CHECK(Stats.SingleDraws == 4 && Stats.InstancedItems == 0 && Stats.FailedItems == 0);
	HYP_CHECK(Stats.Fallbacks[static_cast<std::size_t>(ERenderBatchFallback::Preparation)] == 4);
	Caps.Features[static_cast<std::size_t>(ERHIFeature::InstancedDrawing)].bEnabled = false;
	FRenderBatchSystem Ordinary(InFixture.Tasks, Caps);
	const auto Singles = Prepare(InFixture, Ordinary, Items);
	HYP_CHECK(DrawCount(Fallback) == 4 && InstanceCount(Fallback) == 4);
	HYP_CHECK(InFixture.Draw(Fallback).Rgba == InFixture.Draw(Singles).Rgba);
	ClearBatches(InFixture, Batches, Ordinary);
}
} // namespace

void RunMaterialExecutionGpuTests(FFixture& InFixture)
{
	CheckCustomProgram(InFixture);
	CheckCustomNativeFallback(InFixture);
}
} // namespace Hyperion::InstanceTests
