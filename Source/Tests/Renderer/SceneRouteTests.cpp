#include "Support/SceneRouteTestSupport.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

namespace Hyperion
{
namespace
{
void WriteRouteShader()
{
	const std::string_view Source = R"SHADER(struct FRouteVertex
{
	float4 Position : SV_Position;
};

FRouteVertex RouteVertex(float3 InPosition : POSITION)
{
	FRouteVertex Result;
	Result.Position = float4(InPosition, 1);
	return Result;
}

float4 RouteHdr(FRouteVertex InInput) : SV_Target0
{
	return float4(.25, 1, 3, 1);
}

float4 RouteDisplay(FRouteVertex InInput) : SV_Target0
{
	return float4(.7, .2, .1, 1);
}

struct FRouteGBuffer
{
	float4 BaseMetallic : SV_Target0;
	float4 Normals : SV_Target1;
	float4 Surface : SV_Target2;
	float4 Emissive : SV_Target3;
};

FRouteGBuffer RouteBase(FRouteVertex InInput)
{
	FRouteGBuffer Result;
	Result.BaseMetallic = float4(0, 0, 0, 1);
	Result.Normals = float4(.5, .5, .5, .5);
	Result.Surface = float4(1, 1, 1, 0);
	Result.Emissive = float4(.25, 1, 3, 0);
	return Result;
}
)SHADER";
	FLocalFileSystem Files;
	Files.WriteAtomic(TestShaderRoot() / "SceneRouteTest.hlsl", std::as_bytes(std::span(Source)));
}

FRenderResourceDesc RouteGeometry(FShaderCompiler& InCompiler, const FSceneRouteCase& InCase)
{
	FMaterialDescription Description;
	Description.Name = "route matrix";
	for (const auto& Usage : InCase.Authored)
	{
		FMaterialPass Pass;
		Pass.Usage = Usage;
		Pass.Vertex = {"SceneRouteTest.hlsl", "RouteVertex"};
		Pass.Pixel = {"SceneRouteTest.hlsl", Usage == "DeferredBase" ? "RouteBase"
		                                     : Usage == "Forward"    ? "RouteDisplay"
		                                                             : "RouteHdr"};
		Pass.Queue = Usage == "HdrTransparent" ? EMaterialQueue::Transparent : EMaterialQueue::Opaque;
		Pass.State.bDepthWrite = Usage == "DeferredBase";
		Pass.State.bDepthTest = Usage == "DeferredBase";
		Pass.State.bViewRelativeDepth = true;
		Description.Passes.push_back(std::move(Pass));
	}
	const auto Definition = std::make_shared<const FMaterialDefinition>(std::move(Description));
	const auto Program = std::make_shared<const FCompiledMaterialDefinition>(
	    CompileMaterialDefinition(InCompiler, Definition, EShaderFormat::Dxil));
	FRenderResourceDesc Result;
	FRenderGeometryDesc Geometry;
	const std::array Vertices{FVec3{-.75f, -.75f, .5f}, FVec3{.75f, -.75f, .5f}, FVec3{.75f, .75f, .5f},
	                          FVec3{-.75f, .75f, .5f}};
	Geometry.Vertices.resize(sizeof(Vertices));
	std::memcpy(Geometry.Vertices.data(), Vertices.data(), sizeof(Vertices));
	Geometry.VertexStride = sizeof(FVec3);
	Geometry.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0}};
	Geometry.Indices = {0, 1, 2, 0, 2, 3};
	Geometry.Bounds = {{-.75f, -.75f, .5f}, {.75f, .75f, .5f}, true};
	Result.Geometries.push_back(std::move(Geometry));
	Result.Materials.push_back({FMaterialInstance(Program->Interface).Freeze(), Program});
	Result.Sections.push_back({0, 0, 0, 6});
	return Result;
}

bool RouteGeometryReady(const FRenderResource& InResource)
{
	const auto ResourceError = InResource.GetError();
	if (!ResourceError.empty())
	{
		throw std::runtime_error(ResourceError);
	}
	const auto Material = InResource.GetMaterial(0);
	if (!Material)
	{
		return false;
	}
	const auto MaterialError = Material->GetError();
	if (!MaterialError.empty())
	{
		throw std::runtime_error(MaterialError);
	}
	return InResource.GetStatus() == ERenderResourceStatus::Ready &&
	       Material->GetStatus() == ERenderMaterialStatus::Ready;
}

std::shared_ptr<const FRenderResource> RequestRouteGeometry(FSceneRouteFixture& InFixture,
                                                            const FSceneRouteCase& InCase)
{
	const auto Resource =
	    InFixture.Session.GetResources().Request(std::make_shared<const int>(1), 1, "scene-route-matrix",
	                                             [&Compiler = InFixture.Compiler, InCase]
	                                             {
		                                             return RouteGeometry(Compiler, InCase);
	                                             });
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (!RouteGeometryReady(*Resource) && std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (!Resource->GetError().empty())
	{
		throw std::runtime_error(Resource->GetError());
	}
	HYP_CHECK(Resource->GetStatus() == ERenderResourceStatus::Ready);
	HYP_CHECK(Resource->GetMaterial(0));
	HYP_CHECK(Resource->GetMaterial(0)->GetStatus() == ERenderMaterialStatus::Ready);
	return Resource;
}

void CheckRouteStatistics(const FForwardPipelineStatistics& InStats, ESceneRenderPipeline InPipeline,
                          const std::vector<std::string>& InExpected)
{
	const std::vector<std::pair<std::string, std::uint64_t>> Deferred{
	    {"DeferredBase", 1}, {"HdrCompatibility", 2}, {"HdrTransparent", 3}, {"Forward", 4}};
	const std::vector<std::pair<std::string, std::uint64_t>> Forward{
	    {"HdrForwardOpaque", 1}, {"HdrTransparent", 3}, {"Forward", 4}};
	const auto& Rows = InPipeline == ESceneRenderPipeline::Deferred ? Deferred : Forward;
	HYP_CHECK(InStats.Views.size() == Rows.size());
	for (std::size_t Index = 0; Index < Rows.size(); ++Index)
	{
		const auto& Actual = InStats.Views[Index];
		const auto Count = std::count(InExpected.begin(), InExpected.end(), Rows[Index].first);
		HYP_CHECK(Actual.Identity == Rows[Index].second && Actual.Usage == Rows[Index].first);
		HYP_CHECK(Actual.Visibility.VisibleItems == static_cast<std::size_t>(Count));
		HYP_CHECK(Actual.Visibility.Draws == static_cast<std::size_t>(Count));
		HYP_CHECK(Actual.Visibility.Batches.FailedItems == 0);
	}
	HYP_CHECK(InStats.MainView().Draws == InExpected.size());
}

void CheckRouteCommands(const std::vector<std::shared_ptr<const FPassCommands>>& InCommands,
                        const std::vector<std::string>& InExpected)
{
	const std::array<std::pair<std::string_view, std::string_view>, 5> Prefixes{
	    {{"Deferred/BasePass", "DeferredBase"},
	     {"Forward/HDR", "HdrForwardOpaque"},
	     {"Deferred/Compatibility", "HdrCompatibility"},
	     {"Scene/Transparent", "HdrTransparent"},
	     {"Display/LegacyMaterials", "Forward"}}};
	std::vector<std::string> Actual;
	const void* Geometry{};
	for (const auto& Commands : InCommands)
	{
		HYP_CHECK(Commands);
		for (const auto& Prefix : Prefixes)
		{
			if (!Commands->Name.starts_with(Prefix.first))
			{
				continue;
			}
			const auto Draws = Commands->GetDraws();
			const auto Count = std::count(InExpected.begin(), InExpected.end(), Prefix.second);
			HYP_CHECK(Draws.size() == static_cast<std::size_t>(Count));
			HYP_CHECK(Commands->GetColors().size() == (Prefix.second == "DeferredBase" ? 4 : 1));
			for (const auto& Draw : Draws)
			{
				HYP_CHECK(Draw.IndexCount == 6 && Draw.InstanceCount == 1);
				HYP_CHECK(!Geometry || Geometry == Draw.Vertices.Payload.get());
				Geometry = Draw.Vertices.Payload.get();
				Actual.emplace_back(Prefix.second);
			}
		}
	}
	HYP_CHECK(Actual == InExpected);
}

float RoutePixel(const FImage& InImage, std::size_t InChannel)
{
	return InImage.Rgba.at(((InImage.Height / 2) * std::size_t(InImage.Width) + InImage.Width / 2) * 4 + InChannel);
}

void CheckRoutePixels(const FImage& InImage, const FImage& InEmpty, const std::vector<std::string>& InExpected)
{
	if (InExpected.empty())
	{
		for (std::size_t Channel = 0; Channel < 4; ++Channel)
		{
			HYP_CHECK(std::abs(RoutePixel(InImage, Channel) - RoutePixel(InEmpty, Channel)) < .005f);
		}
		return;
	}
	const std::array Display{.7f, .2f, .1f};
	const std::array Hdr{.484529f, .735357f, .880825f}; // sRGB(.25/1.25, 1/2, 3/4).
	const auto& Expected = InExpected.back() == "Forward" ? Display : Hdr;
	for (std::size_t Channel = 0; Channel < Expected.size(); ++Channel)
	{
		HYP_CHECK(std::abs(RoutePixel(InImage, Channel) - Expected[Channel]) < .015f);
	}
}

void CheckCustomViewCommands(std::span<const FPassCommands> InCommands)
{
	HYP_CHECK(InCommands.size() == 2 && InCommands[0].GetDraws().size() == 1);
	HYP_CHECK(InCommands[0].Draws.empty() && InCommands[0].SharedDraws);
	HYP_CHECK(InCommands[0].GetDraws()[0].IndexCount == 6 && InCommands[0].GetDraws()[0].InstanceCount == 1);
	const auto& Exports = InCommands.back();
	HYP_CHECK(Exports.Name == "Graph exports" && Exports.GetDraws().empty());
	HYP_CHECK(Exports.GetColors().empty() && !Exports.DepthStencil);
	HYP_CHECK(Exports.Transitions.size() == 1);
	const auto& Transition = Exports.Transitions.front();
	HYP_CHECK(Transition.Target == FRenderTarget::Backbuffer());
	HYP_CHECK(Transition.Before == EResourceState::RenderTarget && Transition.After == EResourceState::Present);
}

void CheckExplicitCustomView(FSceneRouteFixture& InFixture)
{
	const auto Frame = InFixture.Session.FreezeFrame();
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    auto View = InFixture.View;
		    View.Identity = 50;
		    View.Usage = "CustomOnly";
		    View.bSkipMissingPass = true;
		    FRenderGraph Graph;
		    InFixture.Session.BuildViews(Graph, std::span(&View, 1),
		                                 InFixture.Session.FrameTargets(FVec4{}, View.DepthConvention), Frame);
		    InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Rhi, 0},
		                                                  [&]
		                                                  {
			                                                  const auto Commands = Graph.CompileAndConsume();
			                                                  CheckCustomViewCommands(Commands);
		                                                  }));
		    const auto Stats = InFixture.Session.GetViewPreparation().Statistics();
		    HYP_CHECK(Stats.Views[0].Usage == "CustomOnly" && Stats.Views[0].Visibility.Draws == 1);
	    }));
}
} // namespace

void RunSceneRouteTests(FSceneRouteFixture InFixture)
{
	WriteRouteShader();
	const auto SavedView = InFixture.View;
	InFixture.View.CullingMode = ESceneCullingMode::None;
	InFixture.View.bInstanceBatching = false;
	for (const auto Pipeline : {ESceneRenderPipeline::Deferred, ESceneRenderPipeline::Forward})
	{
		std::vector<std::shared_ptr<const FPassCommands>> Commands;
		const auto Empty = InFixture.Frame(Pipeline, Commands);
		for (const auto& Case : SceneRouteCases())
		{
			const auto Resource = RequestRouteGeometry(InFixture, Case);
			FRenderPrimitiveState State;
			State.Resource = Resource;
			auto Binding = InFixture.Session.GetScene().Create(State);
			InFixture.Tasks.Wait(InFixture.Session.GetScene().Flush());
			const auto Image = InFixture.Frame(Pipeline, Commands);
			const auto& Expected = Pipeline == ESceneRenderPipeline::Deferred ? Case.DeferredDraws : Case.ForwardDraws;
			CheckRouteStatistics(InFixture.Statistics, Pipeline, Expected);
			CheckRouteCommands(Commands, Expected);
			CheckRoutePixels(Image, Empty, Expected);
			if (Case.Authored == std::vector<std::string>{"CustomOnly"})
			{
				CheckExplicitCustomView(InFixture);
			}
			Commands.clear();
			InFixture.Tasks.Wait(Binding.Remove());
		}
	}
	InFixture.View = SavedView;
}
} // namespace Hyperion
