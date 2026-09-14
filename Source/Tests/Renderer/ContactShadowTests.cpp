#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/ContactShadows.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <fstream>
#include <iostream>

namespace
{
using namespace Hyperion;

std::vector<std::byte> RenderMask(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain,
                                  FRenderSession& InSession,
                                  const std::shared_ptr<const FMaterialDefinition>& InMaterial,
                                  EDepthConvention InConvention, bool bInOccluder)
{
	constexpr FSize Size{255, 129};
	const auto Scope = InSession.GetResources().CreateScopeLifetime();
	const auto Depth = std::make_shared<const FMaterialTextureSource>(
	    FMaterialDepthTexture{Size.Width, Size.Height, GetDepthClearValue(InConvention)});
	const auto Normals = std::make_shared<const FMaterialTextureSource>(
	    FMaterialColorTexture{Size.Width, Size.Height, EMaterialColorFormat::Rgba16Float});
	const auto Coverage = std::make_shared<const FMaterialTextureSource>(
	    FMaterialColorTexture{Size.Width, Size.Height, EMaterialColorFormat::Rgba8Unorm});
	const auto Mask = std::make_shared<const FMaterialTextureSource>(
	    FMaterialColorTexture{Size.Width, Size.Height, EMaterialColorFormat::R8Unorm});
	FContactShadowInputs Inputs;
	Inputs.Normals = {ERenderTargetKind::Texture, Normals, Scope, false};
	Inputs.Surface = {ERenderTargetKind::Texture, Coverage, Scope, false};
	Inputs.Mask = {ERenderTargetKind::Texture, Mask, Scope, false};
	Inputs.View.Width = Size.Width;
	Inputs.View.Height = Size.Height;
	Inputs.View.Eye = {0, 0, -2};
	Inputs.View.ViewProjection = ClipDepthTransform(InConvention);
	Inputs.View.DepthConvention = InConvention;
	Inputs.LightDirection = {.6f, 0, -.8f};
	Inputs.Settings.Steps = 192;
	Inputs.Settings.Thickness = .03f;
	FHierarchicalDepthProducer Producer;
	InTasks.Wait(InTasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderGraph Graph;
		    FFullscreenPassDesc Fixture;
		    Fixture.Material = InMaterial;
		    Fixture.Lifetime = Scope;
		    Fixture.Viewport = {0, 0, float(Size.Width), float(Size.Height)};
		    Fixture.Targets.Name = "Contact fixture";
		    Fixture.Targets.Colors = {{Inputs.Normals, {EAttachmentLoad::Clear}},
		                              {Inputs.Surface, {EAttachmentLoad::Clear}}};
		    Fixture.Targets.DepthStencil = FRenderDepthTarget{{ERenderTargetKind::Texture, Depth, Scope, false},
		                                                      ERHIDepthFormat::D32,
		                                                      FAttachmentActions{EAttachmentLoad::Clear},
		                                                      {},
		                                                      GetDepthClearValue(InConvention)};
		    Fixture.Parameters = {
		        {"Pixel:Fixture.bReversed", FMaterialValue::Uint(InConvention == EDepthConvention::Reversed ? 1 : 0)},
		        {"Pixel:Fixture.bOccluder", FMaterialValue::Uint(bInOccluder ? 1 : 0)}};
		    AddFullscreenPass(InSession, Graph, std::move(Fixture));
		    Inputs.Depth =
		        Producer.Request(InSession, Graph, {{ERenderTargetKind::Texture, Depth, Scope, false}, Inputs.View});
		    AddFullscreenPass(InSession, Graph, MakeContactShadowPass(Graph, Inputs));
		    auto Preview = MakeScreenTexturePreview(Inputs.Mask, {0, 0, 64, 64}, 0, false);
		    Preview.Targets.Color->Actions.Load = EAttachmentLoad::Clear;
		    AddFullscreenPass(InSession, Graph, std::move(Preview));
		    ExecuteGraph(std::move(Graph), InTasks, InSwapchain, {64, 64}, false, false);
	    }));
	std::vector<std::byte> Result;
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              Result = InDevice.ReadTexture(
		                                  {InSession.GetResources().GetPreparation().ResolveTexture(Mask, Scope), 0, 1},
		                                  EResourceState::ShaderRead);
		                              HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
	                              }));
	return Result;
}

void Run()
{
	const auto Root = TestShaderRoot();
	std::ofstream(Root / "ContactFixture.hlsl") << R"(
cbuffer Fixture:register(b0) { uint bReversed; uint bOccluder; };
struct FOutput { float4 Normals:SV_Target0; float4 Coverage:SV_Target1; float Depth:SV_Depth; };
FOutput PSMain(float4 InPosition:SV_Position)
{
    float2 World = InPosition.xy / float2(255,129) * 2 - 1;
    bool bBlock = bOccluder && World.x >= 0 && World.x < .12 && abs(World.y) < .3;
    float Depth = bBlock ? .6 : .7;
    FOutput Result;
    Result.Normals = 1; // Octahedral -Z, both shading and geometric normal.
    Result.Coverage = float4(.5,1,abs(World.y) < .85 ? 1 : 0,0);
    Result.Depth = bReversed ? 1 - Depth : Depth;
    return Result;
})";
	FTaskSystem Tasks(2, 2);
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	auto Device = Registry.CreateDevice(ERHIBackend::D3D12, {});
	FWindow Window("Contact shadow validation", {64, 64}, true);
	auto Swapchain = Device->CreateSwapchain({Window.Surface(), Window.PixelSize()});
	FShaderCompiler Compiler(Root, "contact-test-cache");
	FRenderSession Session(Tasks, *Device, Compiler);
	{
		FMaterialDescription Description;
		Description.Name = "Contact fixture";
		FMaterialPass Pass;
		Pass.Vertex = {"Common/Fullscreen.hlsl", "VSMain"};
		Pass.Pixel = {"ContactFixture.hlsl", "PSMain"};
		Pass.State.bDepthTest = true;
		Pass.State.bDepthWrite = true;
		Pass.State.DepthCompare = EMaterialCompare::Always;
		Description.Passes.push_back(Pass);
		const auto Material = std::make_shared<const FMaterialDefinition>(Description);
		std::vector<std::byte> Standard;
		for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
		{
			const auto Empty = RenderMask(Tasks, *Device, *Swapchain, Session, Material, Convention, false);
			HYP_CHECK(std::all_of(Empty.begin(), Empty.end(),
			                      [](auto InValue)
			                      {
				                      return InValue == std::byte{255};
			                      }));
			const auto Contact = RenderMask(Tasks, *Device, *Swapchain, Session, Material, Convention, true);
			std::uint32_t ShadowPixels{};
			for (std::uint32_t Y = 0; Y < 129; ++Y)
			{
				for (std::uint32_t X = 0; X < 255; ++X)
				{
					const auto Value = std::to_integer<unsigned>(Contact[Y * 255 + X]);
					if (Value < 128)
					{
						++ShadowPixels;
					}
					// Far from the occluder and uncovered pixels must stay fully lit.
					if (X < 100 || X > 145 || Y < 40 || Y > 90)
					{
						HYP_CHECK(Value == 255);
					}
				}
			}
			std::cout << "contact pixels " << ShadowPixels << '\n';
			HYP_CHECK(ShadowPixels > 150 && ShadowPixels < 600);
			HYP_CHECK(Contact[64 * 255 + 123] == std::byte{0});
			if (Convention == EDepthConvention::Standard)
			{
				Standard = Contact;
			}
			else
			{
				HYP_CHECK(Contact == Standard);
			}
		}
	}
	Session.Close();
}
} // namespace

int main()
{
	try
	{
		Run();
		std::cout << "Analytic contact, coplanar rejection, uncovered pixels and dual depth equivalence passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
