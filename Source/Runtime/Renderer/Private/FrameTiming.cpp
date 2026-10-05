#include "Hyperion/Renderer/FrameTiming.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Hyperion
{
void FFrameTimingWindow::RemoveOldest()
{
	TotalMilliseconds -= Samples[First];
	First = (First + 1) % MaximumSamples;
	--Count;
}

void FFrameTimingWindow::Record(double InMilliseconds)
{
	if (!std::isfinite(InMilliseconds) || InMilliseconds <= 0)
	{
		return;
	}
	if (Count == MaximumSamples)
	{
		RemoveOldest();
	}
	Samples[(First + Count) % MaximumSamples] = InMilliseconds;
	++Count;
	TotalMilliseconds += InMilliseconds;
	// Keep complete intervals, including the frame crossing the window boundary.
	while (Count > 1 && TotalMilliseconds - Samples[First] >= FFrameIntervalStatistics::MeasurementWindowMilliseconds)
	{
		RemoveOldest();
	}
}

FFrameIntervalStatistics FFrameTimingWindow::Snapshot() const
{
	FFrameIntervalStatistics Result;
	if (!Count)
	{
		return Result;
	}
	Result.SampleCount = Count;
	Result.CoveredMilliseconds = TotalMilliseconds;
	Result.AverageMilliseconds = TotalMilliseconds / double(Count);
	Result.FramesPerSecond = 1000.0 * double(Count) / TotalMilliseconds;
	Result.LastMilliseconds = Samples[(First + Count - 1) % MaximumSamples];
	Result.bCapacityLimited = Count == MaximumSamples && TotalMilliseconds < Result.TargetWindowMilliseconds;
	std::vector<double> Ordered;
	Ordered.reserve(Count);
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		const double Interval = Samples[(First + Index) % MaximumSamples];
		Ordered.push_back(Interval);
		Result.MaxMilliseconds = std::max(Result.MaxMilliseconds, Interval);
		if (Interval > Result.LongFrameThresholdMilliseconds)
		{
			++Result.LongFrameCount;
		}
	}
	const auto Rank = (95 * Count + 99) / 100 - 1;
	std::nth_element(Ordered.begin(), Ordered.begin() + Rank, Ordered.end());
	Result.P95Milliseconds = Ordered[Rank];
	return Result;
}

template<> const FRecordDescriptor& RecordType<FFrameIntervalStatistics>()
{
	static const auto Type = MakeRecord<FFrameIntervalStatistics>(
	    "hyperion.diagnostics.frameintervalstatistics",
	    {Member("targetWindowMilliseconds", &FFrameIntervalStatistics::TargetWindowMilliseconds),
	     Member("coveredMilliseconds", &FFrameIntervalStatistics::CoveredMilliseconds),
	     Member("sampleCount", &FFrameIntervalStatistics::SampleCount),
	     Member("averageMilliseconds", &FFrameIntervalStatistics::AverageMilliseconds),
	     Member("framesPerSecond", &FFrameIntervalStatistics::FramesPerSecond),
	     Member("lastMilliseconds", &FFrameIntervalStatistics::LastMilliseconds),
	     Member("p95Milliseconds", &FFrameIntervalStatistics::P95Milliseconds),
	     Member("maxMilliseconds", &FFrameIntervalStatistics::MaxMilliseconds),
	     Member("longFrameThresholdMilliseconds", &FFrameIntervalStatistics::LongFrameThresholdMilliseconds),
	     Member("longFrameCount", &FFrameIntervalStatistics::LongFrameCount),
	     Member("capacityLimited", &FFrameIntervalStatistics::bCapacityLimited)});
	return Type;
}
} // namespace Hyperion
