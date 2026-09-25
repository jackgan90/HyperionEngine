#pragma once
#include "Hyperion/Platform/Window.h"
#include "Hyperion/Renderer/RenderPipelineFrame.h"
#include <filesystem>

namespace Hyperion
{
struct FRenderBenchmarkSample
{
	std::uint64_t Frame{};
	double Milliseconds{};
	std::size_t Draws{};
	std::size_t VisibleItems{};
	FRenderBatchStats Batches;
	FForwardPipelineStatistics Pipeline;
	FDeviceStats Device;
	double CpuLatencyMilliseconds{};
	std::size_t Nodes{};
	double SceneMilliseconds{};
	double GuiMilliseconds{};
	double RenderMilliseconds{};
	FSize Viewport;
	FViewport CaptureViewport;
	std::uint32_t MainRenderLead{};
	std::uint32_t RenderRhiLead{};
	double LoadMilliseconds{};
};

// Joins delayed GPU results by submitted frame identity, rejecting missing/ambiguous samples.
void MatchBenchmarkTimings(std::span<FRenderBenchmarkSample> InSamples, FGpuTimingCapture InCapture);
void WriteRenderBenchmark(const std::filesystem::path& InPath, std::span<const FRenderBenchmarkSample> InSamples);
// Deterministic orbit workload; host supplies frame zero after readiness/warmup.
std::array<FInputEvent, 3> BenchmarkCameraInput(std::uint64_t InFrame, float InStep);
FVec3 BenchmarkLightDirection(std::uint64_t InFrame);
} // namespace Hyperion
