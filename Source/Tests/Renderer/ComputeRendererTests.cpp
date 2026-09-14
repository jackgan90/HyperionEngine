#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/ComputePass.h"
#include "Hyperion/Renderer/FullscreenPass.h"
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

FDeviceStats CheckOutput(IRHIDevice& InDevice, FRenderSession& InSession,
                         const std::shared_ptr<const FMaterialTextureSource>& InSource,
                         const std::shared_ptr<const void>& InScope, float InExpected)
{
	const auto Texture = InSession.GetResources().GetPreparation().ResolveTexture(InSource, InScope);
	const auto Bytes = InDevice.ReadTexture({Texture, 0, 1}, EResourceState::ShaderRead);
	for (std::size_t Offset = 0; Offset < Bytes.size(); Offset += 4)
	{
		float Value{};
		std::memcpy(&Value, Bytes.data() + Offset, 4);
		HYP_CHECK(Value == InExpected);
	}
	const auto Stats = InDevice.Statistics();
	HYP_CHECK(Stats.ValidationErrors == 0);
	return Stats;
}

std::filesystem::path WriteComputeShaders()
{
	const auto Root = TestShaderRoot();
	std::ofstream(Root / "ComputeProducer.hlsl") << R"(
cbuffer Parameters : register(b0) { float Value; };
RWTexture2D<float> Output : register(u0);
RWStructuredBuffer<float> Values : register(u1);
[numthreads(4,2,1)] void CSMain(uint3 Id : SV_DispatchThreadID) { if (all(Id.xy < uint2(19,7))) { Output[Id.xy] = Value; Values[Id.y * 19 + Id.x] = Value; } }
[numthreads(8,4,1)] void Alternate(uint3 Id : SV_DispatchThreadID) { if (all(Id.xy < uint2(19,7))) { Output[Id.xy] = Value * 2; Values[Id.y * 19 + Id.x] = Value * 2; } }
[numthreads(8,4,1)] void Accumulate(uint3 Id : SV_DispatchThreadID) { if (all(Id.xy < uint2(19,7))) { Output[Id.xy] += Value; Values[Id.y * 19 + Id.x] += Value; } }
)";
	std::ofstream(Root / "ComputeConsumer.hlsl")
	    << "Texture2D<float> Source:register(t0); StructuredBuffer<float> Values:register(t1); float4 "
	       "PSMain():SV_Target0 {return float4(Source.Load(int3(18,6,0)),Values[132],0,1);}";
	return Root;
}

void CheckComputeRenderer()
{
	const auto Root = WriteComputeShaders();
	FTaskSystem Tasks(2, 2);
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	auto Device = Registry.CreateDevice(ERHIBackend::D3D12, {});
	FWindow Window("Renderer compute tests", {64, 64}, true);
	auto Swapchain = Device->CreateSwapchain({Window.Surface(), Window.PixelSize()});
	FShaderCompiler Compiler(Root, "compute-renderer-cache");
	FRenderSession Session(Tasks, *Device, Compiler);
	{
		const auto Scope = Session.GetResources().CreateScopeLifetime();
		auto Source = std::make_shared<const FMaterialTextureSource>(FMaterialStorageTexture{19, 7});
		FMaterialBufferView View{std::make_shared<const FMaterialReadBufferSource>(19U * 7U * 4U),
		                         EMaterialBufferViewKind::Structured, 0, 19U * 7U * 4U, 4};
		const auto Consumer = MakeFullscreenMaterial("Compute consumer", "ComputeConsumer.hlsl");
		FComputePassDesc Pass;
		Pass.Name = "Compute/Producer";
		Pass.Shader.Source = "ComputeProducer.hlsl";
		Pass.Lifetime = Scope;
		Pass.Extent = {19, 7, 1};
		Pass.Textures = {{"Output", Source, 0, 1, EResourceState::ShaderWrite, true, false}};
		Pass.Buffers = {{"Values", View, EResourceState::ShaderWrite, true, false}};
		FDeviceStats Before;
		// Change constants, resource identities and shader entry independently, then reuse and accumulate.
		for (std::uint32_t Frame = 0; Frame < 6; ++Frame)
		{
			if (Frame == 2)
			{
				Source = std::make_shared<const FMaterialTextureSource>(FMaterialStorageTexture{19, 7});
				View.Source = std::make_shared<const FMaterialReadBufferSource>(19U * 7U * 4U);
				Pass.Textures[0].Source = Source;
				Pass.Buffers[0].View = View;
			}
			Pass.Parameters = {{"Parameters.Value", FMaterialValue::Float(Frame == 5   ? .125f
			                                                              : Frame == 0 ? .25f
			                                                                           : .375f)}};
			Pass.Shader.Entry = Frame == 5 ? "Accumulate" : Frame < 3 ? "CSMain" : "Alternate";
			const auto Expected = Frame == 5 ? .875f : (Frame == 0 ? .25f : .375f) * (Frame < 3 ? 1.f : 2.f);
			Pass.Textures[0].bInitialized = Frame == 5;
			Pass.Textures[0].bFullOverwrite = Frame != 5;
			Pass.Buffers[0].bInitialized = Frame == 5;
			Pass.Buffers[0].bFullOverwrite = Frame != 5;
			Tasks.Wait(Tasks.Dispatch(
			    {EDomain::Render},
			    [&]
			    {
				    FRenderGraph Graph;
				    AddComputePass(Session, Graph, Pass);
				    Pass.Parameters[0].second =
				        FMaterialValue::Float(100.f); // Published parameters must remain unchanged.
				    FFullscreenPassDesc Fullscreen;
				    Fullscreen.Material = Consumer;
				    Fullscreen.Lifetime = Scope;
				    Fullscreen.Viewport = {0, 0, 64, 64};
				    Fullscreen.Targets = FRenderPassTargets::ColorOnly(FVec4{});
				    Fullscreen.Targets.Name = "Compute/Consumer";
				    Fullscreen.Targets.Reads = {{ERenderTargetKind::Texture, Source, Scope, Frame == 5}};
				    Fullscreen.Targets.BufferReads = {{View, Scope, Frame == 5}};
				    Fullscreen.Parameters = {{"Pixel:Source", FMaterialValue::FromTexture(Source)},
				                             {"Pixel:Values", FMaterialValue::FromBuffer(View)}};
				    AddFullscreenPass(Session, Graph, std::move(Fullscreen));
				    const auto Image = ExecuteGraph(std::move(Graph), Tasks, *Swapchain, {64, 64}, false, true);
				    HYP_CHECK(std::abs(Image.Rgba[0] - Expected) < .006f);
				    HYP_CHECK(std::abs(Image.Rgba[1] - Expected) < .006f);
			    }));
			Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                          [&]
			                          {
				                          const auto Stats = CheckOutput(*Device, Session, Source, Scope, Expected);
				                          if (Frame == 4)
				                          {
					                          HYP_CHECK(Stats.PipelinesCreated == Before.PipelinesCreated &&
					                                    Stats.DescriptorAllocations == Before.DescriptorAllocations);
				                          }
				                          Before = Stats;
			                          }));
		}
	}
	Session.Close();
}
} // namespace

int main()
{
	try
	{
		CheckComputeRenderer();
		std::cout << "Compute named parameters, shader replacement, publication and graphics consumption passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
