#include "Hyperion/Reflection/Json.h"
#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/Renderer/FrameTiming.h"
#include "Hyperion/Renderer/RenderDiagnostics.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <limits>

namespace
{
using namespace Hyperion;

void EmptyAndInvalidIntervals()
{
	FFrameTimingWindow Window;
	Window.Record(0);
	Window.Record(-1);
	Window.Record(std::numeric_limits<double>::infinity());
	Window.Record(std::numeric_limits<double>::quiet_NaN());
	const auto Empty = Window.Snapshot();
	HYP_CHECK(!Empty.SampleCount && !Empty.CoveredMilliseconds && !Empty.FramesPerSecond);
	HYP_CHECK(!Empty.LastMilliseconds && !Empty.P95Milliseconds && !Empty.MaxMilliseconds);
	HYP_CHECK(!Empty.LongFrameCount && !Empty.bCapacityLimited);
	Window.Record(4);
	Window.Record(-20);
	const auto Single = Window.Snapshot();
	HYP_CHECK(Single.SampleCount == 1 && Single.CoveredMilliseconds == 4);
	HYP_CHECK(Single.LastMilliseconds == 4 && Single.P95Milliseconds == 4 && Single.MaxMilliseconds == 4);
}

void ThroughputAndPercentile()
{
	FFrameTimingWindow Window;
	Window.Record(4);
	Window.Record(20);
	const auto Mixed = Window.Snapshot();
	HYP_CHECK(Mixed.SampleCount == 2 && Mixed.CoveredMilliseconds == 24);
	HYP_CHECK(Mixed.AverageMilliseconds == 12 && std::abs(Mixed.FramesPerSecond - 1000.0 / 12) < 1e-10);
	HYP_CHECK(Mixed.LastMilliseconds == 20 && Mixed.MaxMilliseconds == 20 && Mixed.P95Milliseconds == 20);
	HYP_CHECK(Mixed.LongFrameCount == 1);
	FFrameTimingWindow Ranked;
	for (int Interval = 20; Interval >= 1; --Interval)
	{
		Ranked.Record(Interval);
	}
	const auto Percentile = Ranked.Snapshot();
	HYP_CHECK(Percentile.P95Milliseconds == 19 && Percentile.MaxMilliseconds == 20);
	HYP_CHECK(Percentile.LastMilliseconds == 1 && Percentile.LongFrameCount == 4);
	FFrameTimingWindow Threshold;
	Threshold.Record(FFrameIntervalStatistics::LongFrameMilliseconds);
	Threshold.Record(FFrameIntervalStatistics::LongFrameMilliseconds + .001);
	HYP_CHECK(Threshold.Snapshot().LongFrameCount == 1);
}

void RollingSpikeRetention()
{
	FFrameTimingWindow Window;
	for (int Index = 0; Index < 250; ++Index)
	{
		Window.Record(4);
	}
	HYP_CHECK(Window.Snapshot().SampleCount == 250 && Window.Snapshot().CoveredMilliseconds == 1000);
	Window.Record(20);
	const auto Spike = Window.Snapshot();
	HYP_CHECK(Spike.SampleCount == 246 && Spike.CoveredMilliseconds == 1000);
	HYP_CHECK(Spike.P95Milliseconds == 4 && Spike.MaxMilliseconds == 20 && Spike.LongFrameCount == 1);
	for (int Index = 0; Index < 249; ++Index)
	{
		Window.Record(4);
	}
	HYP_CHECK(Window.Snapshot().SampleCount == 250 && Window.Snapshot().CoveredMilliseconds == 1016);
	HYP_CHECK(Window.Snapshot().MaxMilliseconds == 20);
	Window.Record(4);
	const auto Expired = Window.Snapshot();
	HYP_CHECK(Expired.SampleCount == 250 && Expired.CoveredMilliseconds == 1000);
	HYP_CHECK(Expired.MaxMilliseconds == 4 && !Expired.LongFrameCount);
}

void LongStallsAndCapacity()
{
	FFrameTimingWindow Stall;
	Stall.Record(4);
	Stall.Record(2500);
	HYP_CHECK(Stall.Snapshot().SampleCount == 1 && Stall.Snapshot().CoveredMilliseconds == 2500);
	for (int Index = 0; Index < 249; ++Index)
	{
		Stall.Record(4);
	}
	HYP_CHECK(Stall.Snapshot().MaxMilliseconds == 2500 && Stall.Snapshot().CoveredMilliseconds == 3496);
	Stall.Record(4);
	HYP_CHECK(Stall.Snapshot().MaxMilliseconds == 4 && Stall.Snapshot().CoveredMilliseconds == 1000);
	FFrameTimingWindow Fast;
	for (std::size_t Index = 0; Index < FFrameTimingWindow::MaximumSamples * 3; ++Index)
	{
		Fast.Record(.125);
	}
	const auto Limited = Fast.Snapshot();
	HYP_CHECK(Limited.SampleCount == FFrameTimingWindow::MaximumSamples && Limited.bCapacityLimited);
	HYP_CHECK(Limited.CoveredMilliseconds == 512 && Limited.FramesPerSecond == 8000);
	Fast.Record(1000);
	HYP_CHECK(Fast.Snapshot().SampleCount == 1 && !Fast.Snapshot().bCapacityLimited);
}

void ReflectedDiagnostics()
{
	FFrameTimingWindow Window;
	Window.Record(4);
	Window.Record(20);
	FRenderDiagnostics Diagnostics;
	Diagnostics.FrameIntervalMilliseconds = 20;
	Diagnostics.FrameIntervalStatistics = Window.Snapshot();
	const auto& Type = RecordType<FRenderDiagnostics>();
	HYP_CHECK(Type.Id == "hyperion.render.diagnostics" && Type.Version == 1);
	const auto Wire = WriteRecordWire(Type, &Diagnostics);
	const auto Loaded = ReadRecordWire(Type, Wire);
	const auto& Restored = *static_cast<const FRenderDiagnostics*>(Loaded.get());
	HYP_CHECK(Restored.FrameIntervalMilliseconds == 20);
	HYP_CHECK(Restored.FrameIntervalStatistics.AverageMilliseconds == 12);
	HYP_CHECK(Restored.FrameIntervalStatistics.SampleCount == 2);
	HYP_CHECK(Restored.FrameIntervalStatistics.LongFrameCount == 1);
	HYP_CHECK(WriteJson(WriteRecordWire(Type, Loaded.get())) == WriteJson(Wire));
	const auto Schema = WriteJson(RecordWireSchema(Type));
	HYP_CHECK(Schema.find("frameIntervalStatistics") != std::string::npos);
	HYP_CHECK(Schema.find("hyperion.diagnostics.frameintervalstatistics") != std::string::npos);
	HYP_CHECK(Schema.find("longFrameThresholdMilliseconds") != std::string::npos);
}
} // namespace

void RunFrameTimingTests()
{
	EmptyAndInvalidIntervals();
	ThroughputAndPercentile();
	RollingSpikeRetention();
	LongStallsAndCapacity();
	ReflectedDiagnostics();
}
