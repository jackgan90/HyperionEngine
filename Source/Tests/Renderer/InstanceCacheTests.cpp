#include "Renderer/InstanceBatchSupport.h"

namespace Hyperion::InstanceTests
{
void RunInstanceCacheTests(FFixture& InFixture)
{
	const auto Original = InFixture.Build();
	const auto Pixels = InFixture.Draw(Original);
	const auto Same = InFixture.Build();
	HYP_CHECK(InFixture.Statistics.Batches.ReusedChunks == 2 && InFixture.Statistics.Batches.UploadBytes == 0);
	HYP_CHECK(InFixture.Statistics.Batches.GpuReuses > 0);
	InFixture.Emission->Changed = 6;
	const auto Changed = InFixture.Build();
	HYP_CHECK(InFixture.Statistics.Batches.ReusedChunks == 1 && InFixture.Statistics.Batches.RebuiltChunks == 1);
	HYP_CHECK(InFixture.Draw(Changed).Rgba != Pixels.Rgba);
	HYP_CHECK(InFixture.Draw(Original).Rgba == Pixels.Rgba);
	HYP_CHECK(InFixture.Draw(Changed).Rgba == InFixture.Draw(InFixture.Build(false)).Rgba);
	InFixture.Build();
	InFixture.Emission->Hidden = 6;
	const auto Hidden = InFixture.Build();
	HYP_CHECK(InstanceCount(Hidden) == 7 && DrawCount(Hidden) == 2);
	HYP_CHECK(InFixture.Statistics.Batches.RebuiltChunks == 1);
	HYP_CHECK(InFixture.Draw(Hidden).Rgba == InFixture.Draw(InFixture.Build(false)).Rgba);
	InFixture.Emission->Hidden.reset();
	InFixture.Emission->Changed.reset();
	InFixture.Emission->bReverse = true;
	const auto Reversed = InFixture.Build();
	HYP_CHECK(InFixture.Statistics.Batches.RebuiltChunks == 2);
	HYP_CHECK(InFixture.Draw(Reversed).Rgba == Pixels.Rgba);
	InFixture.Emission->bStable = false;
	InFixture.Build();
	InFixture.Build();
	HYP_CHECK(InFixture.Statistics.Batches.ReusedChunks == 0 && InFixture.Statistics.Batches.RebuiltChunks == 2);
	InFixture.Emission->bStable = true;
	InFixture.Emission->bReverse = false;
	const std::array<FRenderView, 2> Views{InFixture.View, [&]
	                                       {
		                                       auto View = InFixture.View;
		                                       View.Identity = 2;
		                                       return View;
	                                       }()};
	const auto Multi = InFixture.Build(true, Views);
	HYP_CHECK(InstanceCount(Multi) == 16 && DrawCount(Multi) == 4);
	HYP_CHECK(InFixture.Statistics.Batches.InstancedItems == 16);
}
} // namespace Hyperion::InstanceTests
