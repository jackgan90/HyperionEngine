#include "DirectionalLighting.h"
#include "Hyperion/Materials/Lighting/ClusterParameters.h"
#include "Hyperion/Renderer/ClusteredLights.h"
#include "Hyperion/Renderer/ComputePass.h"
#include "Hyperion/Renderer/RenderSession.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>

namespace Hyperion::RendererPrivate
{
namespace
{
constexpr std::array<std::uint32_t, 16> PointWords{
    0x3fa00000U, 0xc0200000U, 0xc0700000U, 0x3e000000U, 0x40000000U, 0x40800000U, 0x41000000U, 0x00000000U,
    0x3ef5c28fU, 0xbf23d70aU, 0x3f19999aU, 0x3f600000U, 0x3ec00000U, 0x00000000U, 0x00000000U, 0x00000000U};
constexpr std::array<std::uint32_t, 16> SpotWords{
    0xc0900000U, 0x40a80000U, 0xc0d80000U, 0x3d800000U, 0x3f000000U, 0x3fc00000U, 0x40200000U, 0x3f800000U,
    0xbeb851ecU, 0x3ef5c28fU, 0xbf4ccccdU, 0x3f400000U, 0x3e800000U, 0x00000000U, 0x00000000U, 0x00000000U};
constexpr std::array<std::uint32_t, 16> DirectionalWords{
    0x3ef5c28fU, 0xbf23d70aU, 0x3f19999aU, 0x00000000U, 0x3f800000U, 0x40000000U, 0x40400000U, 0x00000000U,
    0xbeb851ecU, 0x3ef5c28fU, 0xbf4ccccdU, 0x00000000U, 0x40800000U, 0x3f800000U, 0x40200000U, 0x00000000U};
constexpr std::array<std::uint32_t, 8> EmptyDirectionalWords{0, 0, 0x3f800000U, 0, 0, 0, 0, 0};

void Check(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

std::vector<std::uint32_t> ReadWords(std::span<const std::byte> InBytes)
{
	Check(InBytes.size() % 4 == 0, "Lighting wire source contains whole uint32 words");
	std::vector<std::uint32_t> Result;
	for (std::size_t Offset = 0; Offset < InBytes.size(); Offset += 4)
	{
		Result.push_back(std::to_integer<std::uint32_t>(InBytes[Offset]) |
		                 (std::to_integer<std::uint32_t>(InBytes[Offset + 1]) << 8) |
		                 (std::to_integer<std::uint32_t>(InBytes[Offset + 2]) << 16) |
		                 (std::to_integer<std::uint32_t>(InBytes[Offset + 3]) << 24));
	}
	return Result;
}

void CheckWords(const FMaterialBufferView& InView, std::span<const std::uint32_t> InExpected, std::uint32_t InStride)
{
	Check(InView.Source && InView.Kind == EMaterialBufferViewKind::Structured && InView.Offset == 0 &&
	          InView.Stride == InStride && InView.Size == InExpected.size() * 4,
	      "Lighting producer publishes the fixed structured extent and stride");
	Check(std::ranges::equal(ReadWords(InView.Source->GetBytes()), InExpected),
	      "Lighting producer words differ from independent fixed values");
}

const FMaterialBufferView& ClusterBuffer(const FClusterLightFrame& InFrame, EClusterSemantic InSemantic)
{
	const auto Found = std::find_if(InFrame.Parameters.begin(), InFrame.Parameters.end(),
	                                [&](const auto& InParameter)
	                                {
		                                return InParameter.GetSemantic() == InSemantic;
	                                });
	Check(Found != InFrame.Parameters.end(), "Cluster fixture resource is present");
	return Found->Value.Buffer;
}

FRenderView FixtureView()
{
	FRenderView View;
	View.Width = 64;
	View.Height = 64;
	View.Camera = FRenderCamera{{0, 0, -1}, {0, 1, 0}, 1, .1f, 40};
	View.ViewProjection = Perspective(1, 1, .1f, 40);
	return View;
}

std::array<FLocalLight, 2> FixtureLights()
{
	FLocalLight Point;
	Point.Position = {1.25f, -2.5f, -3.75f};
	Point.Radiance = {2, 4, 8};
	Point.Direction = {.48f, -.64f, .6f};
	Point.Range = 8;
	Point.InnerCos = .875f;
	Point.OuterCos = .375f;
	Point.Bounds = {{-64, -64, -64}, {64, 64, 64}, true};
	FLocalLight Spot;
	Spot.Position = {-4.5f, 5.25f, -6.75f};
	Spot.Radiance = {.5f, 1.5f, 2.5f};
	Spot.Direction = {-.36f, .48f, -.8f};
	Spot.Range = 16;
	Spot.InnerCos = .75f;
	Spot.OuterCos = .25f;
	Spot.bSpot = true;
	Spot.Bounds = Point.Bounds;
	return {Point, Spot};
}

FSceneMetadata FixtureDirectionalLights()
{
	FSceneMetadata Metadata;
	Metadata.Lighting.Directional.Handle = FSceneHandle{1, 1, 1};
	Metadata.DirectionalLights.emplace(FSceneHandle{1, 5, 1},
	                                   FPublishedDirectionalLight{{{2, .5f, 1.25f}, 2}, {-.36f, .48f, -.8f}, true});
	Metadata.DirectionalLights.emplace(FSceneHandle{1, 1, 1},
	                                   FPublishedDirectionalLight{{{8, 8, 8}, 2}, {1, 0, 0}, true});
	Metadata.DirectionalLights.emplace(FSceneHandle{1, 3, 1},
	                                   FPublishedDirectionalLight{{{9, 8, 7}, 0}, {0, 1, 0}, true});
	Metadata.DirectionalLights.emplace(FSceneHandle{1, 4, 1},
	                                   FPublishedDirectionalLight{{{.25f, .5f, .75f}, 4}, {.48f, -.64f, .6f}, true});
	Metadata.DirectionalLights.emplace(FSceneHandle{1, 2, 1},
	                                   FPublishedDirectionalLight{{{7, 5, 3}, 4}, {0, 0, 1}, false});
	return Metadata;
}

std::vector<std::uint32_t> ExpectedClusterWords()
{
	std::vector<std::uint32_t> Result(PointWords.begin(), PointWords.end());
	Result.insert(Result.end(), SpotWords.begin(), SpotWords.end());
	return Result;
}

std::vector<std::uint32_t> ExpectedHeaderWords()
{
	std::vector<std::uint32_t> Result;
	for (std::uint32_t Cell = 0; Cell < 24; ++Cell)
	{
		Result.insert(Result.end(), {Cell * 2, 2});
	}
	return Result;
}

std::vector<std::uint32_t> ExpectedIndexWords()
{
	std::vector<std::uint32_t> Result;
	for (std::uint32_t Cell = 0; Cell < 24; ++Cell)
	{
		Result.insert(Result.end(), {0, 1});
	}
	return Result;
}

void CheckEmptySources()
{
	const FClusterLightFrame Empty{DefaultClusterParameters(), {}};
	CheckWords(ClusterBuffer(Empty, EClusterSemantic::ClusterLights), std::array<std::uint32_t, 16>{}, 64);
	CheckWords(ClusterBuffer(Empty, EClusterSemantic::ClusterHeaders), std::array<std::uint32_t, 2>{}, 8);
	CheckWords(ClusterBuffer(Empty, EClusterSemantic::ClusterIndices), std::array<std::uint32_t, 1>{}, 4);
	const auto Directional = DirectionalLightBuffer();
	CheckWords(Directional.Buffer, EmptyDirectionalWords, 32);
	Check(DirectionalLightBuffer().Buffer.Source == Directional.Buffer.Source, "Default directional source is shared");
	const FSceneMetadata Metadata;
	const auto ExplicitEmpty = DirectionalLightBuffer(&Metadata, &Directional);
	Check(ExplicitEmpty.Buffer.Source == Directional.Buffer.Source,
	      "Explicit empty directional bytes reuse the previous source");
}

void CheckClusterSources()
{
	auto Lights = FixtureLights();
	auto View = FixtureView();
	FClusteredLights Builder;
	const auto Original = Builder.Build(View, Lights);
	const auto& OriginalLights = ClusterBuffer(Original, EClusterSemantic::ClusterLights);
	const auto& OriginalHeaders = ClusterBuffer(Original, EClusterSemantic::ClusterHeaders);
	const auto& OriginalIndices = ClusterBuffer(Original, EClusterSemantic::ClusterIndices);
	CheckWords(OriginalLights, ExpectedClusterWords(), 64);
	CheckWords(OriginalHeaders, ExpectedHeaderWords(), 8);
	CheckWords(OriginalIndices, ExpectedIndexWords(), 4);
	Check(Original.Statistics.Cells == 24 && Original.Statistics.References == 48,
	      "Controlled coverage produces all 24 cells");
	const auto Same = Builder.Build(View, Lights);
	Check(!Same.Statistics.bRebuilt, "Repeated lighting input reuses cluster assignment");
	for (const auto Semantic :
	     {EClusterSemantic::ClusterLights, EClusterSemantic::ClusterHeaders, EClusterSemantic::ClusterIndices})
	{
		Check(ClusterBuffer(Same, Semantic).Source == ClusterBuffer(Original, Semantic).Source,
		      "Repeated lighting bytes reuse all sources");
	}
	Lights[0].Radiance.X = 9.5f;
	const auto Edited = Builder.Build(View, Lights);
	Check(!Edited.Statistics.bRebuilt &&
	          ClusterBuffer(Edited, EClusterSemantic::ClusterLights).Source != OriginalLights.Source,
	      "Radiance edits replace attributes without rebuilding assignment");
	Check(ClusterBuffer(Edited, EClusterSemantic::ClusterHeaders).Source == OriginalHeaders.Source &&
	          ClusterBuffer(Edited, EClusterSemantic::ClusterIndices).Source == OriginalIndices.Source,
	      "Radiance edits retain header and index sources");
	auto EditedWords = ExpectedClusterWords();
	EditedWords[4] = 0x41180000U;
	CheckWords(ClusterBuffer(Edited, EClusterSemantic::ClusterLights), EditedWords, 64);
	View.Eye.X = .1f;
	View.ViewProjection = Multiply(View.ViewProjection, Translation({-.1f, 0, 0}));
	const auto Moved = Builder.Build(View, Lights);
	Check(Moved.Statistics.bRebuilt &&
	          ClusterBuffer(Moved, EClusterSemantic::ClusterHeaders).Source == OriginalHeaders.Source &&
	          ClusterBuffer(Moved, EClusterSemantic::ClusterIndices).Source == OriginalIndices.Source,
	      "Recomputed identical assignment bytes retain their sources");
	Lights[0].Bounds.Maximum.X += 1;
	Check(Builder.Build(View, Lights).Statistics.bRebuilt, "Bounds changes rebuild assignment");
	Builder.Reset();
	CheckWords(OriginalLights, ExpectedClusterWords(), 64);
	CheckWords(OriginalHeaders, ExpectedHeaderWords(), 8);
	CheckWords(OriginalIndices, ExpectedIndexWords(), 4);
	CheckWords(ClusterBuffer(Edited, EClusterSemantic::ClusterLights), EditedWords, 64);
}

void CheckDirectionalSources()
{
	auto Metadata = FixtureDirectionalLights();
	const auto Original = DirectionalLightBuffer(&Metadata);
	CheckWords(Original.Buffer, DirectionalWords, 32);
	const auto Same = DirectionalLightBuffer(&Metadata, &Original);
	Check(Same.Buffer.Source == Original.Buffer.Source, "Identical eligible directional bytes reuse the source");
	Metadata.DirectionalLights.at({1, 5, 1}).Light.Color.X = 3;
	const auto Edited = DirectionalLightBuffer(&Metadata, &Original);
	Check(Edited.Buffer.Source != Original.Buffer.Source, "Changed eligible directional radiance replaces its source");
	auto EditedWords = DirectionalWords;
	EditedWords[12] = 0x40c00000U;
	CheckWords(Edited.Buffer, EditedWords, 32);
	Metadata.DirectionalLights.clear();
	const auto Empty = DirectionalLightBuffer(&Metadata, &Edited);
	CheckWords(Empty.Buffer, EmptyDirectionalWords, 32);
	Check(DirectionalLightBuffer(&Metadata, &Empty).Buffer.Source == Empty.Buffer.Source,
	      "Identical empty directional bytes reuse the previous source");
	CheckWords(Original.Buffer, DirectionalWords, 32);
	CheckWords(Edited.Buffer, EditedWords, 32);
}

std::vector<std::uint32_t> ExpectedReadback(bool bInEmpty)
{
	if (bInEmpty)
	{
		std::vector<std::uint32_t> Result{1, 64, 1, 8, 1, 4, 1, 32};
		Result.resize(8 + 16 + 2 + 1, 0);
		Result.insert(Result.end(), EmptyDirectionalWords.begin(), EmptyDirectionalWords.end());
		return Result;
	}
	std::vector<std::uint32_t> Result{2, 64, 24, 8, 48, 4, 2, 32};
	for (const auto& Words : {ExpectedClusterWords(), ExpectedHeaderWords(), ExpectedIndexWords()})
	{
		Result.insert(Result.end(), Words.begin(), Words.end());
	}
	Result.insert(Result.end(), DirectionalWords.begin(), DirectionalWords.end());
	return Result;
}

void ReadbackSources(FTaskSystem& InTasks, FRenderSession& InSession, IRHIDevice& InDevice, IRHISwapchain& InSwapchain,
                     const std::shared_ptr<const void>& InScope, bool bInEmpty)
{
	FClusteredLights Builder;
	const auto Lights = FixtureLights();
	const auto Cluster =
	    bInEmpty ? FClusterLightFrame{DefaultClusterParameters(), {}} : Builder.Build(FixtureView(), Lights);
	const auto Metadata = FixtureDirectionalLights();
	const auto Directional = DirectionalLightBuffer(bInEmpty ? nullptr : &Metadata);
	const auto Expected = ExpectedReadback(bInEmpty);
	const FMaterialBufferView Output{std::make_shared<const FMaterialReadBufferSource>(Expected.size() * 4),
	                                 EMaterialBufferViewKind::Structured, 0, Expected.size() * 4, 4};
	FComputePassDesc Pass;
	Pass.Name = bInEmpty ? "Lighting wire defaults" : "Lighting wire fields";
	Pass.Shader.Source = "LightingWireReadback.hlsl";
	Pass.Shader.Contracts = {GetClusterShaderContracts(), GetSceneLightingShaderContracts()};
	Pass.Lifetime = InScope;
	Pass.Buffers = {{EClusterSemantic::ClusterLights, ClusterBuffer(Cluster, EClusterSemantic::ClusterLights)},
	                {EClusterSemantic::ClusterHeaders, ClusterBuffer(Cluster, EClusterSemantic::ClusterHeaders)},
	                {EClusterSemantic::ClusterIndices, ClusterBuffer(Cluster, EClusterSemantic::ClusterIndices)},
	                {ESceneLightingSemantic::DirectionalLights, Directional.Buffer},
	                {"WireOutput", Output, EResourceState::ShaderWrite, true, false}};
	InTasks.Wait(
	    InTasks.Dispatch({EDomain::Render},
	                     [&]
	                     {
		                     FRenderGraph Graph;
		                     AddComputePass(InSession, Graph, Pass);
		                     Builder.Reset(); // Graph owns the actual immutable sources after producer retirement.
		                     ExecuteGraph(std::move(Graph), InTasks, InSwapchain, {64, 64}, false, false);
	                     }));
	InTasks.Wait(InTasks.Dispatch(
	    {EDomain::Rhi, 0},
	    [&]
	    {
		    const auto Buffer = InSession.GetResources().GetPreparation().ResolveBuffer(Output.Source, InScope);
		    const FReadBufferView View{Buffer, ERHIBufferViewKind::Structured, 0, Output.Size, 4};
		    Check(ReadWords(InDevice.ReadBuffer(View, EResourceState::ShaderRead)) == Expected,
		          "GPU lighting field reads differ from fixed independent words");
		    Check(InDevice.Statistics().ValidationErrors == 0,
		          "Lighting wire readback has no native validation errors");
	    }));
}
} // namespace

void CheckLightingWireProducerBytes()
{
	CheckEmptySources();
	CheckClusterSources();
	CheckDirectionalSources();
}

void CheckLightingWireReadback(FTaskSystem& InTasks, FRenderSession& InSession, IRHIDevice& InDevice,
                               IRHISwapchain& InSwapchain, const std::shared_ptr<const void>& InScope)
{
	ReadbackSources(InTasks, InSession, InDevice, InSwapchain, InScope, false);
	ReadbackSources(InTasks, InSession, InDevice, InSwapchain, InScope, true);
}
} // namespace Hyperion::RendererPrivate
