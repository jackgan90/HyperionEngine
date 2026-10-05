#pragma once
#include "Hyperion/Reflection/RecordValue.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace Hyperion
{
struct FFrameIntervalStatistics
{
	static constexpr double MeasurementWindowMilliseconds = 1000.0;
	static constexpr double LongFrameMilliseconds = 1000.0 / 60.0;

	double TargetWindowMilliseconds{MeasurementWindowMilliseconds};
	double CoveredMilliseconds{};
	std::uint64_t SampleCount{};
	double AverageMilliseconds{};
	double FramesPerSecond{};
	double LastMilliseconds{};
	double P95Milliseconds{};
	double MaxMilliseconds{};
	double LongFrameThresholdMilliseconds{LongFrameMilliseconds};
	std::uint64_t LongFrameCount{};
	bool bCapacityLimited{};
};

template<> const FRecordDescriptor& RecordType<FFrameIntervalStatistics>();

// Single-owner accumulator; record measured update-start intervals on Main.
class FFrameTimingWindow
{
public:
	static constexpr std::size_t MaximumSamples = 4096;

	void Record(double InMilliseconds);
	FFrameIntervalStatistics Snapshot() const;

private:
	void RemoveOldest();

	std::array<double, MaximumSamples> Samples{};
	std::size_t First{};
	std::size_t Count{};
	double TotalMilliseconds{};
};
} // namespace Hyperion
