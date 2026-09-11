#include "Hyperion/Renderer/FullscreenPass.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <fstream>

using namespace Hyperion;

namespace
{
std::filesystem::path WriteShader()
{
	const auto Root = std::filesystem::absolute("fullscreen-test/source");
	std::filesystem::create_directories(Root / "Common");
	std::filesystem::copy_file(std::filesystem::path(HYP_SOURCE_DIR) / "shaders/Common/Fullscreen.hlsl",
	                           Root / "Common/Fullscreen.hlsl", std::filesystem::copy_options::overwrite_existing);
	std::ofstream(Root / "Values.hlsl") << R"(
cbuffer ValuesV1 : register(b0) { float A; float B; };
float4 PSMain() : SV_Target0 { return float4(A, B, 0, 1); }
)";
	return Root;
}

FMaterialDescription Description()
{
	FMaterialDescription Result;
	Result.Name = "Fullscreen regression";
	FMaterialPass Pass;
	Pass.Vertex = {"Common/Fullscreen.hlsl", "VSMain"};
	Pass.Pixel = {"Values.hlsl", "PSMain"};
	Result.Passes.push_back(Pass);
	for (const auto* Name : {"A", "B"})
	{
		FMaterialParameterDeclaration Parameter;
		Parameter.Name = Name;
		Parameter.Targets = {std::string("ValuesV1.") + Name};
		Parameter.Default = FMaterialValue::Float(0.f);
		Result.Parameters.push_back(Parameter);
	}
	return Result;
}

float Pixel(const FImage& InImage, unsigned InChannel)
{
	return InImage.Rgba.at((144 * InImage.Width + 192) * 4 + InChannel);
}

struct FFullscreenFixture
{
	FTaskSystem& Tasks;
	IRHISwapchain& Swapchain;
	FShaderCompiler Compiler{WriteShader(), "fullscreen-test/cache"};
	FRenderSession Session;
	FFullscreenPassDesc Pass;

	FFullscreenFixture(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
	    : Tasks(InTasks), Swapchain(InSwapchain), Session(InTasks, InDevice, Compiler)
	{
		Pass.Material = std::make_shared<const FMaterialDefinition>(Description());
		Pass.Lifetime = Session.GetResources().CreateScopeLifetime();
		Pass.Viewport = {0, 0, 384, 288};
		Pass.Targets = FRenderPassTargets::ColorOnly(FVec4{});
	}

	~FFullscreenFixture()
	{
		Session.Close();
	}

	FImage Render(FMaterialParameterValues InValues, bool bInDeferPreparation = true)
	{
		Pass.Parameters = std::move(InValues);
		FImage Image;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          FRenderGraph Graph;
			                          AddFullscreenPass(Session, Graph, Pass, bInDeferPreparation);
			                          Image = ExecuteGraph(std::move(Graph), Tasks, Swapchain, {384, 288}, false, true);
		                          }));
		return Image;
	}
};

void CheckRecovery(FFullscreenFixture& InFixture)
{
	const auto Before = InFixture.Render({{"A", FMaterialValue::Float(.25f)}});
	HYP_CHECK(std::abs(Pixel(Before, 0) - .25f) < .008f);
	HYP_CHECK(Pixel(Before, 1) == 0);
	for (const bool bDeferred : {false, true})
	{
		for (const bool bWrongType : {false, true})
		{
			bool bRejected = false;
			try
			{
				const FMaterialParameterEntry Invalid =
				    bWrongType ? FMaterialParameterEntry{"A", FMaterialValue::Uint(1)}
				               : FMaterialParameterEntry{"Unknown", FMaterialValue::Float(0.f)};
				InFixture.Render({{"B", FMaterialValue::Float(.75f)}, Invalid}, bDeferred);
			}
			catch (const std::invalid_argument&)
			{
				bRejected = true;
			}
			HYP_CHECK(bRejected);
			const auto After = InFixture.Render({{"A", FMaterialValue::Float(.25f)}}, bDeferred);
			HYP_CHECK(After.Rgba == Before.Rgba);
		}
	}
	// A valid omitted parameter must also return to its declared default.
	InFixture.Render({{"B", FMaterialValue::Float(.75f)}});
	HYP_CHECK(InFixture.Render({{"A", FMaterialValue::Float(.25f)}}).Rgba == Before.Rgba);
}

void CheckDynamicState(FFullscreenFixture& InFixture)
{
	auto Material = Description();
	Material.Passes[0].State.bBlend = true;
	Material.Passes[0].State.SourceRgb = EMaterialBlendFactor::Constant;
	Material.Passes[0].DynamicState.BlendConstants = {.25f, .25f, .25f, .25f};
	Material.Passes[0].DynamicState.StencilReference = 37;
	InFixture.Pass.Material = std::make_shared<const FMaterialDefinition>(std::move(Material));
	const auto Image = InFixture.Render({{"A", FMaterialValue::Float(1.f)}});
	HYP_CHECK(std::abs(Pixel(Image, 0) - .25f) < .008f);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Rhi, 0},
	    [&]
	    {
		    const auto Batch = InFixture.Session.GetResources().GetPreparation().BuildFullscreen(InFixture.Pass);
		    HYP_CHECK(Batch.Commands.Draws.at(0).DynamicState.StencilReference == 37);
	    }));
}
} // namespace

void RunFullscreenTests(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	FFullscreenFixture Fixture(InTasks, InDevice, InSwapchain);
	CheckRecovery(Fixture);
	CheckDynamicState(Fixture);
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
	                              }));
}
