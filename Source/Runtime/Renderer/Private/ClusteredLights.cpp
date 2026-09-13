#include "Hyperion/Renderer/ClusteredLights.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace Hyperion
{
namespace
{
constexpr std::uint32_t TileSize = 64;
constexpr std::uint32_t DepthSlices = 24;
constexpr std::size_t MaximumCells = 1024 * 1024;
constexpr std::size_t MaximumReferences = 16 * 1024 * 1024;
constexpr std::size_t MaximumLights = 65536;
static_assert(sizeof(FClusterLightData) == 64 && sizeof(FClusterHeader) == 8);

FMaterialValue BufferValue(const std::shared_ptr<const FMaterialReadBufferSource>& InSource, std::uint32_t InStride)
{
	return FMaterialValue::FromBuffer(
	    {InSource, EMaterialBufferViewKind::Structured, 0, InSource->GetBytes().size(), InStride});
}

template<typename T> std::shared_ptr<const FMaterialReadBufferSource> Buffer(std::span<const T> InValues)
{
	const T Empty{};
	return std::make_shared<const FMaterialReadBufferSource>(
	    std::as_bytes(InValues.empty() ? std::span(&Empty, 1) : InValues));
}

template<typename T>
std::shared_ptr<const FMaterialReadBufferSource> ReuseBuffer(
    std::span<const T> InValues, const std::shared_ptr<const FMaterialReadBufferSource>& InPrevious)
{
	const T Empty{};
	const auto Bytes = std::as_bytes(InValues.empty() ? std::span(&Empty, 1) : InValues);
	if (InPrevious && std::ranges::equal(Bytes, InPrevious->GetBytes()))
	{
		return InPrevious;
	}
	return Buffer<T>(InValues);
}

struct FGrid
{
	FViewport Viewport;
	FVec3 Forward;
	std::uint32_t X{};
	std::uint32_t Y{};
	float Near{};
	float Far{};
	float DepthScale{};
	float DepthBias{};
};

FGrid Grid(const FRenderView& InView)
{
	if (!InView.Camera)
	{
		throw std::invalid_argument("Cluster lighting requires perspective camera metadata");
	}
	FGrid Result;
	Result.Viewport = InView.Viewport.value_or(FViewport{0, 0, float(InView.Width), float(InView.Height)});
	Result.Near = InView.Camera->Near;
	Result.Far = InView.Camera->Far;
	if (!(Result.Near > 0) || !(Result.Far > Result.Near) || !std::isfinite(Result.Far) ||
	    !(Result.Viewport.Width > 0) || !(Result.Viewport.Height > 0) ||
	    !std::isfinite(Result.Viewport.Width + Result.Viewport.Height) || Result.Viewport.Width > 65536 ||
	    Result.Viewport.Height > 65536)
	{
		throw std::invalid_argument("Invalid cluster camera range or viewport");
	}
	Result.Forward = Normalize(InView.Camera->Forward);
	if (!std::isfinite(Result.Forward.X + Result.Forward.Y + Result.Forward.Z) || Length(Result.Forward) < .9f)
	{
		throw std::invalid_argument("Invalid cluster camera forward direction");
	}
	Result.X = static_cast<std::uint32_t>(std::ceil(Result.Viewport.Width / TileSize));
	Result.Y = static_cast<std::uint32_t>(std::ceil(Result.Viewport.Height / TileSize));
	Result.DepthScale = DepthSlices / (std::log2(Result.Far) - std::log2(Result.Near));
	Result.DepthBias = -std::log2(Result.Near) * Result.DepthScale;
	if (std::size_t(Result.X) * Result.Y * DepthSlices > MaximumCells ||
	    !std::isfinite(Result.DepthScale + Result.DepthBias))
	{
		throw std::invalid_argument("Cluster grid exceeds supported budget");
	}
	return Result;
}

std::uint32_t Slice(float InDepth, const FGrid& InGrid)
{
	return static_cast<std::uint32_t>(
	    std::clamp(std::floor(std::log2(std::max(InDepth, InGrid.Near)) * InGrid.DepthScale + InGrid.DepthBias), 0.f,
	               float(DepthSlices - 1)));
}

struct FCoverage
{
	std::uint32_t MinX{};
	std::uint32_t MaxX{};
	std::uint32_t MinY{};
	std::uint32_t MaxY{};
	std::uint32_t MinZ{};
	std::uint32_t MaxZ{};
	bool bVisible{};
};

FCoverage Coverage(const FBounds& InBounds, const FRenderView& InView, const FGrid& InGrid)
{
	float MinDepth = std::numeric_limits<float>::max();
	float MaxDepth = -MinDepth;
	FVec2 Minimum{MinDepth, MinDepth};
	FVec2 Maximum{MaxDepth, MaxDepth};
	bool bCrossesEye{};
	for (unsigned Corner = 0; Corner < 8; ++Corner)
	{
		const FVec3 Point{Corner & 1 ? InBounds.Maximum.X : InBounds.Minimum.X,
		                  Corner & 2 ? InBounds.Maximum.Y : InBounds.Minimum.Y,
		                  Corner & 4 ? InBounds.Maximum.Z : InBounds.Minimum.Z};
		const float Depth = Dot(Subtract(Point, InView.Eye), InGrid.Forward);
		MinDepth = std::min(MinDepth, Depth);
		MaxDepth = std::max(MaxDepth, Depth);
		const auto Clip = Transform(InView.ViewProjection, {Point.X, Point.Y, Point.Z, 1});
		if (!(Clip.W > 1e-6f))
		{
			bCrossesEye = true;
			continue;
		}
		const float X = (Clip.X / Clip.W * .5f + .5f) * InGrid.Viewport.Width;
		const float Y = (.5f - Clip.Y / Clip.W * .5f) * InGrid.Viewport.Height;
		Minimum = {std::min(Minimum.X, X), std::min(Minimum.Y, Y)};
		Maximum = {std::max(Maximum.X, X), std::max(Maximum.Y, Y)};
	}
	// Expand both depth and screen bounds at cell boundaries; near/eye crossings use conservative full XY.
	const float Margin = std::max(1e-4f, std::max(std::abs(MinDepth), std::abs(MaxDepth)) * 1e-5f);
	MinDepth -= Margin;
	MaxDepth += Margin;
	if (MaxDepth < InGrid.Near || MinDepth > InGrid.Far)
	{
		return {};
	}
	if (bCrossesEye || MinDepth <= InGrid.Near)
	{
		Minimum = {};
		Maximum = {InGrid.Viewport.Width, InGrid.Viewport.Height};
	}
	if (Maximum.X < 0 || Maximum.Y < 0 || Minimum.X > InGrid.Viewport.Width || Minimum.Y > InGrid.Viewport.Height)
	{
		return {};
	}
	const auto Tile = [](float InPixel, std::uint32_t InCount)
	{
		return static_cast<std::uint32_t>(std::clamp(std::floor(InPixel / TileSize), 0.f, float(InCount - 1)));
	};
	return {Tile(Minimum.X - .01f, InGrid.X),
	        Tile(Maximum.X + .01f, InGrid.X),
	        Tile(Minimum.Y - .01f, InGrid.Y),
	        Tile(Maximum.Y + .01f, InGrid.Y),
	        Slice(MinDepth, InGrid),
	        Slice(MaxDepth, InGrid),
	        true};
}

template<typename TVisit> void Visit(const FCoverage& InRange, const FGrid& InGrid, TVisit InVisit)
{
	if (!InRange.bVisible)
	{
		return;
	}
	for (auto Z = InRange.MinZ; Z <= InRange.MaxZ; ++Z)
	{
		for (auto Y = InRange.MinY; Y <= InRange.MaxY; ++Y)
		{
			for (auto X = InRange.MinX; X <= InRange.MaxX; ++X)
			{
				InVisit(X + InGrid.X * (Y + InGrid.Y * Z));
			}
		}
	}
}

struct FLists
{
	std::vector<FClusterHeader> Headers;
	std::vector<std::uint32_t> Indices;
};

FLists BuildLists(const FRenderView& InView, const FGrid& InGrid, std::span<const FLocalLight> InLights)
{
	FLists Result;
	Result.Headers.resize(std::size_t(InGrid.X) * InGrid.Y * DepthSlices);
	std::vector<FCoverage> Ranges;
	std::size_t References{};
	for (const auto& Light : InLights)
	{
		Ranges.push_back(Coverage(Light.Bounds, InView, InGrid));
		Visit(Ranges.back(), InGrid,
		      [&](std::uint32_t InIndex)
		      {
			      if (++References > MaximumReferences)
			      {
				      throw std::length_error("Cluster light references exceed 16M budget; no lights were truncated");
			      }
			      ++Result.Headers[InIndex].Count;
		      });
	}
	std::uint32_t Offset{};
	for (auto& Header : Result.Headers)
	{
		Header.Offset = Offset;
		Offset += Header.Count;
		Header.Count = 0;
	}
	Result.Indices.resize(References);
	for (std::uint32_t Light = 0; Light < Ranges.size(); ++Light)
	{
		Visit(Ranges[Light], InGrid,
		      [&](std::uint32_t InIndex)
		      {
			      auto& Header = Result.Headers[InIndex];
			      Result.Indices[Header.Offset + Header.Count++] = Light;
		      });
	}
	return Result;
}
} // namespace

FMaterialParameterValues DefaultClusterParameters()
{
	static const auto Light = Buffer<FClusterLightData>({});
	static const auto Header = Buffer<FClusterHeader>({});
	static const auto Indices = Buffer<std::uint32_t>({});
	return {{"Engine.View.ClusterViewport", FMaterialValue::Float(FVec4{})},
	        {"Engine.View.ClusterGrid", FMaterialValue::Float(FVec4{})},
	        {"Engine.View.ClusterCamera", FMaterialValue::Float(FVec4{})},
	        {"Engine.View.ClusterForward", FMaterialValue::Float(FVec4{})},
	        {"Engine.View.ClusterDepth", FMaterialValue::Float(FVec4{})},
	        {"Engine.View.ClusterLights", BufferValue(Light, 64)},
	        {"Engine.View.ClusterHeaders", BufferValue(Header, 8)},
	        {"Engine.View.ClusterIndices", BufferValue(Indices, 4)}};
}

FClusterLightFrame FClusteredLights::Build(const FRenderView& InView, std::span<const FLocalLight> InLights)
{
	HYP_PERF_SCOPE_C(Render, BuildLightClusters);
	const auto Start = std::chrono::steady_clock::now();
	if (InLights.empty())
	{
		Reset();
		return {DefaultClusterParameters(), {}};
	}
	if (InLights.size() > MaximumLights)
	{
		throw std::length_error("Cluster light count exceeds supported budget");
	}
	const auto Layout = Grid(InView);
	std::vector<float> Key(InView.ViewProjection.Values.begin(), InView.ViewProjection.Values.end());
	Key.insert(Key.end(), {InView.Eye.X, InView.Eye.Y, InView.Eye.Z, Layout.Forward.X, Layout.Forward.Y,
	                       Layout.Forward.Z, Layout.Near, Layout.Far, Layout.Viewport.Width, Layout.Viewport.Height});
	std::vector<FClusterLightData> NewAttributes;
	for (const auto& Light : InLights)
	{
		const auto& Bounds = Light.Bounds;
		Key.insert(Key.end(), {Bounds.Minimum.X, Bounds.Minimum.Y, Bounds.Minimum.Z, Bounds.Maximum.X, Bounds.Maximum.Y,
		                       Bounds.Maximum.Z});
		NewAttributes.push_back({{Light.Position.X, Light.Position.Y, Light.Position.Z, 1.f / Light.Range},
		                         {Light.Radiance.X, Light.Radiance.Y, Light.Radiance.Z, Light.bSpot ? 1.f : 0.f},
		                         {Light.Direction.X, Light.Direction.Y, Light.Direction.Z, Light.InnerCos},
		                         {Light.OuterCos, 0, 0, 0}});
	}
	const bool bRebuild = !HeaderBuffer || AssignmentKey != Key;
	if (bRebuild)
	{
		const auto Lists = BuildLists(InView, Layout, InLights);
		auto Headers = ReuseBuffer<FClusterHeader>(Lists.Headers, HeaderBuffer);
		auto Indices = ReuseBuffer<std::uint32_t>(Lists.Indices, IndexBuffer);
		Statistics = {};
		Statistics.Cells = Lists.Headers.size();
		Statistics.References = Lists.Indices.size();
		for (const auto& Header : Lists.Headers)
		{
			Statistics.Occupied += Header.Count != 0;
			Statistics.MaximumLights = std::max(Statistics.MaximumLights, std::size_t(Header.Count));
		}
		HeaderBuffer = std::move(Headers);
		IndexBuffer = std::move(Indices);
		AssignmentKey = std::move(Key);
	}
	if (!LightBuffer || Attributes.size() != NewAttributes.size() ||
	    std::memcmp(Attributes.data(), NewAttributes.data(), NewAttributes.size() * sizeof(FClusterLightData)) != 0)
	{
		LightBuffer = Buffer<FClusterLightData>(NewAttributes);
		Attributes = std::move(NewAttributes);
	}
	FClusterLightFrame Result;
	const auto& Port = Layout.Viewport;
	Result.Parameters = {
	    {"Engine.View.ClusterViewport", FMaterialValue::Float(FVec4{Port.X, Port.Y, 1.f / TileSize, 1.f / TileSize})},
	    {"Engine.View.ClusterGrid",
	     FMaterialValue::Float(FVec4{float(Layout.X), float(Layout.Y), float(DepthSlices), 1})},
	    {"Engine.View.ClusterCamera", FMaterialValue::Float(FVec4{InView.Eye.X, InView.Eye.Y, InView.Eye.Z, 0})},
	    {"Engine.View.ClusterForward",
	     FMaterialValue::Float(FVec4{Layout.Forward.X, Layout.Forward.Y, Layout.Forward.Z, 0})},
	    {"Engine.View.ClusterDepth",
	     FMaterialValue::Float(FVec4{Layout.DepthScale, Layout.DepthBias, Layout.Near, Layout.Far})},
	    {"Engine.View.ClusterLights", BufferValue(LightBuffer, 64)},
	    {"Engine.View.ClusterHeaders", BufferValue(HeaderBuffer, 8)},
	    {"Engine.View.ClusterIndices", BufferValue(IndexBuffer, 4)}};
	Result.Statistics = Statistics;
	Result.Statistics.bRebuilt = bRebuild;
	Result.Statistics.BufferBytes =
	    LightBuffer->GetBytes().size() + HeaderBuffer->GetBytes().size() + IndexBuffer->GetBytes().size();
	Result.Statistics.BuildMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	return Result;
}

void FClusteredLights::Reset()
{
	AssignmentKey.clear();
	Attributes.clear();
	LightBuffer.reset();
	HeaderBuffer.reset();
	IndexBuffer.reset();
	Statistics = {};
}
} // namespace Hyperion
