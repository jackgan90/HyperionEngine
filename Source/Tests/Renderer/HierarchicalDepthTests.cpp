#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/FullscreenPass.h"
#include "Hyperion/Renderer/HierarchicalDepth.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

namespace
{
using namespace Hyperion;

std::vector<float> ReadFloats(IRHIDevice& InDevice, FTexture InTexture, std::uint32_t InMip)
{
	const auto Bytes = InDevice.ReadTexture({InTexture, InMip, 1}, EResourceState::ShaderRead);
	std::vector<float> Values(Bytes.size() / 4);
	std::memcpy(Values.data(), Bytes.data(), Bytes.size());
	return Values;
}

std::vector<float> Reduce(const std::vector<float>& InSource, FSize InSize, FSize InOutput, bool bInMaximum)
{
	std::vector<float> Result(std::size_t(InOutput.Width) * InOutput.Height, bInMaximum ? 0.f : 1.f);
	for (std::uint32_t Y = 0; Y < InOutput.Height; ++Y)
	{
		for (std::uint32_t X = 0; X < InOutput.Width; ++X)
		{
			const auto Left = static_cast<unsigned>(std::floor(double(X) / InOutput.Width * InSize.Width));
			const auto Right = static_cast<unsigned>(std::ceil(double(X + 1) * InSize.Width / InOutput.Width));
			const auto Top = static_cast<unsigned>(std::floor(double(Y) / InOutput.Height * InSize.Height));
			const auto Bottom = static_cast<unsigned>(std::ceil(double(Y + 1) * InSize.Height / InOutput.Height));
			for (auto Sy = Top; Sy < Bottom; ++Sy)
			{
				for (auto Sx = Left; Sx < Right; ++Sx)
				{
					auto& Value = Result[Y * InOutput.Width + X];
					Value = bInMaximum ? std::max(Value, InSource[Sy * InSize.Width + Sx])
					                   : std::min(Value, InSource[Sy * InSize.Width + Sx]);
				}
			}
		}
	}
	return Result;
}

void CheckProduct(IRHIDevice& InDevice, const FRenderResourcePreparation& InPreparation,
                  const FHierarchicalDepthProduct& InProduct, const std::vector<float>& InBase)
{
	const auto Texture = InPreparation.ResolveTexture(InProduct.Texture, InProduct.Lifetime);
	auto Expected = InBase;
	for (std::uint32_t Mip = 0; Mip < InProduct.MipSizes.size(); ++Mip)
	{
		if (Mip)
		{
			Expected = Reduce(Expected, InProduct.MipSizes[Mip - 1], InProduct.MipSizes[Mip],
			                  (InProduct.Reduction == EDepthReduction::Nearest) ==
			                      (InProduct.Convention == EDepthConvention::Reversed));
		}
		const auto Actual = ReadFloats(InDevice, Texture, Mip);
		HYP_CHECK(Actual.size() == Expected.size());
		for (std::size_t Index = 0; Index < Expected.size(); ++Index)
		{
			HYP_CHECK(std::abs(Actual[Index] - Expected[Index]) < 1e-6f);
		}
	}
}

void CheckProductGeneration(const FHierarchicalDepthProduct& InProduct, const FRenderGraph& InGraph,
                            const FRenderView& InView)
{
	InProduct.Validate(InGraph, InView);
	HYP_CHECK(InProduct.Viewport == *InView.Viewport && InProduct.GraphIdentity == InGraph.GetIdentity());
	FRenderGraph LaterGraph;
	for (unsigned Change = 0; Change < 5; ++Change)
	{
		auto View = InView;
		if (Change == 0)
		{
			++View.Identity;
		}
		else if (Change == 1)
		{
			++View.Revision;
		}
		else if (Change == 2)
		{
			View.ViewProjection.Values[12] += .1f;
		}
		else if (Change == 3)
		{
			View.Viewport->MinDepth += .01f;
		}
		bool bRejected = false;
		try
		{
			InProduct.Validate(Change == 4 ? LaterGraph : InGraph, View);
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
	}
}

void RunCase(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain, FRenderSession& InSession,
             const std::shared_ptr<const FMaterialDefinition>& InMaterial, FSize InSize, EDepthConvention InConvention)
{
	FHierarchicalDepthProducer Producer;
	const auto Scope = InSession.GetResources().CreateScopeLifetime();
	const auto Source = std::make_shared<const FMaterialTextureSource>(
	    FMaterialDepthTexture{InSize.Width, InSize.Height, GetDepthClearValue(InConvention)});
	FHierarchicalDepthRequest Request;
	Request.Depth = {ERenderTargetKind::Texture, Source, Scope, false};
	Request.View.Width = InSize.Width;
	Request.View.Height = InSize.Height;
	Request.View.DepthConvention = InConvention;
	Request.View.Viewport = FViewport{0, 0, float(InSize.Width), float(InSize.Height), .2f, .8f};
	FHierarchicalDepthProduct Nearest;
	FHierarchicalDepthProduct Farthest;
	InTasks.Wait(InTasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderGraph Graph;
		    Producer.BeginFrame(Graph);
		    HYP_CHECK(Producer.Statistics().Dispatches == 0);
		    FFullscreenPassDesc Depth;
		    Depth.Material = InMaterial;
		    Depth.Lifetime = Scope;
		    Depth.Viewport = *Request.View.Viewport;
		    Depth.bFullTargetViewport = true;
		    Depth.Targets.Name = "HZB test depth";
		    Depth.Targets.DepthStencil = FRenderDepthTarget{Request.Depth,
		                                                    ERHIDepthFormat::D32,
		                                                    FAttachmentActions{EAttachmentLoad::Clear},
		                                                    {},
		                                                    GetDepthClearValue(InConvention)};
		    Depth.Parameters = {
		        {"Pixel:Test.bReversed", FMaterialValue::Uint(InConvention == EDepthConvention::Reversed ? 1 : 0)}};
		    AddFullscreenPass(InSession, Graph, std::move(Depth));
		    Nearest = Producer.Request(InSession, Graph, Request);
		    CheckProductGeneration(Nearest, Graph, Request.View);
		    HYP_CHECK(Producer.Request(InSession, Graph, Request).Texture == Nearest.Texture);
		    auto Stale = Request;
		    ++Stale.View.Revision;
		    bool bRejected = false;
		    try
		    {
			    Producer.Request(InSession, Graph, Stale);
		    }
		    catch (const std::invalid_argument&)
		    {
			    bRejected = true;
		    }
		    HYP_CHECK(bRejected);
		    Request.Reduction = EDepthReduction::Farthest;
		    Farthest = Producer.Request(InSession, Graph, Request);
		    Producer.EndFrame();
		    HYP_CHECK(Producer.Statistics().Products == 2 && Producer.Statistics().Consumers == 3);
		    ExecuteGraph(std::move(Graph), InTasks, InSwapchain, {64, 64}, false, false);
	    }));
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              const auto Preparation = InSession.GetResources().GetPreparation();
		                              auto Expected =
		                                  ReadFloats(InDevice, Preparation.ResolveTexture(Source, Scope), 0);
		                              for (auto& Depth : Expected)
		                              {
			                              Depth = std::clamp((Depth - .2f) / .6f, 0.f, 1.f);
		                              }
		                              CheckProduct(InDevice, Preparation, Nearest, Expected);
		                              CheckProduct(InDevice, Preparation, Farthest, Expected);
		                              HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
	                              }));
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              FRenderGraph Next;
		                              Producer.BeginFrame(Next);
		                              Producer.EndFrame();
		                              HYP_CHECK(Producer.Statistics().Products == 0 &&
		                                        Producer.Statistics().Dispatches == 0 &&
		                                        Producer.Statistics().Bytes == 0);
	                              }));
}

void Run()
{
	const auto Root = TestShaderRoot();
	std::ofstream(Root / "HzbTest.hlsl") << R"(
cbuffer Test : register(b0) { uint bReversed; };
float PSMain(float4 InPosition:SV_Position):SV_Depth
{
    uint2 Pixel = uint2(InPosition.xy);
    float Depth = float((Pixel.x * 13 + Pixel.y * 17) % 97) / 96.0;
    return .2 + .6 * (bReversed ? 1 - Depth : Depth);
})";
	FTaskSystem Tasks(2, 2);
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	auto Device = Registry.CreateDevice(ERHIBackend::D3D12, {});
	FWindow Window("Hierarchical depth validation", {64, 64}, true);
	auto Swapchain = Device->CreateSwapchain({Window.Surface(), Window.PixelSize()});
	Swapchain->SetGpuTimingEnabled(true);
	FShaderCompiler Compiler(Root, "hzb-test-cache");
	FRenderSession Session(Tasks, *Device, Compiler);
	{
		FMaterialDescription Description;
		Description.Name = "HZB test depth";
		FMaterialPass Pass;
		Pass.Vertex = {"Common/Fullscreen.hlsl", "VSMain"};
		Pass.Pixel = {"HzbTest.hlsl", "PSMain"};
		Pass.State.bDepthTest = true;
		Pass.State.bDepthWrite = true;
		Pass.State.DepthCompare = EMaterialCompare::Always;
		Pass.State.ColorWriteMask = 0;
		Description.Passes.push_back(Pass);
		const auto Material = std::make_shared<const FMaterialDefinition>(Description);
		for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
		{
			for (const auto Size :
			     {FSize{1, 1}, FSize{1, 17}, FSize{17, 1}, FSize{17, 9}, FSize{32, 32}, FSize{255, 129}})
			{
				RunCase(Tasks, *Device, *Swapchain, Session, Material, Size, Convention);
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
		std::cout << "Requested HZB, all mips, NPOT footprints and both depth conventions passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
