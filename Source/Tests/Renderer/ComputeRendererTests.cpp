#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Materials/Lighting/ClusterParameters.h"
#include "Hyperion/Renderer/ComputePass.h"
#include "Hyperion/Renderer/FullscreenPass.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/ShaderParameters/HierarchicalDepthParameters.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

namespace Hyperion::RendererPrivate
{
void CheckLightingWireReadback(FTaskSystem& InTasks, FRenderSession& InSession, IRHIDevice& InDevice,
                               IRHISwapchain& InSwapchain, const std::shared_ptr<const void>& InScope);
}

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
	std::filesystem::copy_file(std::filesystem::path(HYP_SOURCE_DIR) / "Source/Tests/Shaders/LightingWireReadback.hlsl",
	                           Root / "LightingWireReadback.hlsl", std::filesystem::copy_options::overwrite_existing);
	std::ofstream(Root / "ComputeProducer.hlsl") << R"(
cbuffer FrameInfo : register(b7, space1) { float Time; uint Index; };
#define Value Time
RWTexture2D<float> Output : register(u0);
RWStructuredBuffer<float> Values : register(u1);
[numthreads(4,2,1)] void CSMain(uint3 Id : SV_DispatchThreadID) { if (all(Id.xy < uint2(19,7))) { Output[Id.xy] = Value; Values[Id.y * 19 + Id.x] = Value; } }
[numthreads(8,4,1)] void Alternate(uint3 Id : SV_DispatchThreadID) { if (all(Id.xy < uint2(19,7))) { Output[Id.xy] = Value * 2; Values[Id.y * 19 + Id.x] = Value * 2; } }
[numthreads(8,4,1)] void Accumulate(uint3 Id : SV_DispatchThreadID) { if (all(Id.xy < uint2(19,7))) { Output[Id.xy] += Value; Values[Id.y * 19 + Id.x] += Value; } }
)";
	std::ofstream(Root / "ReadOnlyBufferInput.hlsl") << R"(
StructuredBuffer<float> Input : register(t0);
RWTexture2D<float> Output : register(u0);
[numthreads(1,1,1)] void CSMain(uint3 Id : SV_DispatchThreadID) { Output[Id.xy] = Input[Id.y * 19 + Id.x]; }
)";
	std::ofstream(Root / "ComputeConsumer.hlsl")
	    << "Texture2D<float> Source:register(t0); StructuredBuffer<float> Values:register(t1); float4 "
	       "PSMain():SV_Target0 {return float4(Source.Load(int3(18,6,0)),Values[132],0,1);}";
	std::ofstream(Root / "BadEngineCompute.hlsl") << R"(
cbuffer FrameInfo : register(b0) { uint Time; uint Index; };
RWStructuredBuffer<uint> Output : register(u0);
[numthreads(1,1,1)] void CSMain() { Output[0] = Time; }
)";
	std::ofstream(Root / "OptionalCompute.hlsl")
	    << "#include \"" << GetHierarchicalDepthShaderContracts()->IncludeName << "\"\n"
	    << R"(
cbuffer HZBCopyV1 : register(b0) { FHZBCopyV1Uniform HierarchicalCopy; };
Texture2D<float> SourceDepth : register(t0);
struct FLight { float4 PositionRange; float4 RadianceType; float4 DirectionInner; float4 Outer; };
StructuredBuffer<FLight> ClusterLights : register(t1);
RWTexture2D<float> OutputDepth : register(u0);
[numthreads(1,1,1)] void CSMain(uint3 Id : SV_DispatchThreadID) { OutputDepth[Id.xy] = 1; }
)";
	std::ofstream(Root / "AuthoredAlias.hlsl") << R"(
cbuffer Custom : register(b0) { float ALBEDO_TEXTURE; };
float4 PSMain() : SV_Target0 { return ALBEDO_TEXTURE; }
)";
	std::ofstream(Root / "OptionalConsumer.hlsl")
	    << "Texture2D<float> Source : register(t0); float4 PSMain() : SV_Target0 { return Source.Load(int3(0,0,0)); }";
	return Root;
}

void CheckOptionalComputeGraph(FTaskSystem& InTasks, FRenderSession& InSession, IRHIDevice& InDevice,
                               IRHISwapchain& InSwapchain, const FComputePassDesc& InDescription)
{
	InTasks.Wait(InTasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderGraph Graph;
		    const auto Stats = InDevice.Statistics();
		    const auto Pass = InSession.GetResources().GetPreparation().DeclareCompute(Graph, InDescription);
		    HYP_CHECK(Pass.Reads.empty() && Pass.Writes.size() == 1 && Pass.Buffers.empty());
		    HYP_CHECK(InDevice.Statistics().PipelinesCreated == Stats.PipelinesCreated);
		    AddComputePass(InSession, Graph, InDescription);
		    FFullscreenPassDesc Consumer;
		    Consumer.Material = MakeFullscreenMaterial("Optional consumer", "OptionalConsumer.hlsl");
		    Consumer.Lifetime = InDescription.Lifetime;
		    Consumer.Viewport = {0, 0, 64, 64};
		    Consumer.Targets = FRenderPassTargets::ColorOnly(FVec4{});
		    const auto Source = InDescription.Textures.front().Source;
		    Consumer.Targets.Reads = {{ERenderTargetKind::Texture, Source, InDescription.Lifetime, false}};
		    Consumer.Parameters = {{"Source", FMaterialValue::FromTexture(Source)}};
		    AddFullscreenPass(InSession, Graph, std::move(Consumer));
		    const auto Image = ExecuteGraph(std::move(Graph), InTasks, InSwapchain, {64, 64}, false, true);
		    HYP_CHECK(Image.Rgba.front() > .99f);
	    }));
}

void CheckOptionalCompute(FTaskSystem& InTasks, FRenderSession& InSession, IRHIDevice& InDevice,
                          IRHISwapchain& InSwapchain, const std::shared_ptr<const void>& InScope)
{
	const auto Source = std::make_shared<const FMaterialTextureSource>(FMaterialStorageTexture{1, 1});
	const auto Unused = std::make_shared<const FMaterialTextureSource>(FMaterialStorageTexture{1, 1});
	FComputePassDesc Description;
	Description.Name = "Optional compute";
	Description.Shader.Source = "OptionalCompute.hlsl";
	Description.Shader.Contracts = {GetHierarchicalDepthShaderContracts()};
	Description.Lifetime = InScope;
	Description.EngineParameters = MakeShaderParameters(FHZBCopyV1Parameters{});
	Description.Textures = {{EHierarchicalDepthSemantic::DepthOutput, Source, 0, 1, EResourceState::ShaderWrite, true},
	                        {EHierarchicalDepthSemantic::DepthSource, Unused}};
	Description.Buffers = {{EClusterSemantic::ClusterLights, {}}}; // Absent resources require no source or graph edge.
	CheckOptionalComputeGraph(InTasks, InSession, InDevice, InSwapchain, Description);
	InTasks.Wait(InTasks.Dispatch(
	    {EDomain::Rhi, 0},
	    [&]
	    {
		    const auto Packet = InSession.GetResources().GetPreparation().BuildCompute(Description);
		    HYP_CHECK(Packet.ConstantBindings.empty());
		    const auto Reject = [&](const FComputePassDesc& InBad)
		    {
			    bool bRejected{};
			    try
			    {
				    InSession.GetResources().GetPreparation().BuildCompute(InBad);
			    }
			    catch (const std::invalid_argument&)
			    {
				    bRejected = true;
			    }
			    HYP_CHECK(bRejected);
		    };
		    auto Bad = Description;
		    Bad.Parameters.push_back({"Typo.Unknown", FMaterialValue::Float(1)});
		    Reject(Bad);
		    Bad = Description;
		    Bad.EngineParameters.push_back({FMaterialSemanticId("Game.Unknown"), FMaterialValue::Float(1)});
		    Reject(Bad);
		    Bad = Description;
		    Bad.EngineParameters.push_back(Bad.EngineParameters.front());
		    Reject(Bad);
		    Bad = Description;
		    Bad.Textures.push_back(Bad.Textures.back());
		    Reject(Bad);
		    Bad = Description;
		    Bad.Textures.front().Access = EResourceState::ShaderRead;
		    Reject(Bad);
	    }));
}

void CheckAuthoredFullscreen(FTaskSystem& InTasks, FRenderSession& InSession,
                             const std::shared_ptr<const void>& InScope)
{
	FFullscreenPassDesc Description;
	Description.Material = MakeFullscreenMaterial("Authored alias", "AuthoredAlias.hlsl");
	Description.Lifetime = InScope;
	Description.Viewport = {0, 0, 1, 1};
	Description.Targets = FRenderPassTargets::ColorOnly();
	Description.Parameters = {{"ALBEDO_TEXTURE", FMaterialValue::Float(.5f)}};
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              const auto Batch =
		                                  InSession.GetResources().GetPreparation().BuildFullscreen(Description);
		                              HYP_CHECK(Batch.Commands.Draws.size() == 1);
	                              }));
}

void CheckFullscreenSemanticAliases(FTaskSystem& InTasks, FRenderSession& InSession, IRHISwapchain& InSwapchain,
                                    const std::shared_ptr<const void>& InScope)
{
	auto Registry = std::make_shared<FMaterialSemanticRegistry>();
	Registry->Register({"Game.Amount",
	                    FMaterialParameterType::Numeric(EMaterialScalar::Float),
	                    EMaterialScope::Material,
	                    "Amount",
	                    true,
	                    {"Game.AmountAlias"}});
	auto Material = MakeFullscreenMaterial("Custom semantic aliases", "AuthoredAlias.hlsl")->GetDescription();
	auto Parameter = DeclareMaterialSemantic("Amount", "Game.Amount", *Registry);
	Parameter.Source = EMaterialParameterSource::Manual;
	Parameter.OverridePolicy = EMaterialOverridePolicy::AllowOverride;
	Parameter.Targets = {"Pixel:Custom.ALBEDO_TEXTURE"};
	Material.Parameters.push_back(std::move(Parameter));
	FFullscreenPassDesc Description;
	Description.Material = std::make_shared<const FMaterialDefinition>(std::move(Material), Registry);
	Description.Lifetime = InScope;
	Description.Viewport = {0, 0, 64, 64};
	Description.Targets = FRenderPassTargets::ColorOnly(FVec4{});
	const auto Render = [&](const FMaterialParameterValues& InValues, float InExpected)
	{
		Description.Parameters = InValues;
		InTasks.Wait(InTasks.Dispatch({EDomain::Render},
		                              [&]
		                              {
			                              FRenderGraph Graph;
			                              AddFullscreenPass(InSession, Graph, Description);
			                              const auto Image = ExecuteGraph(std::move(Graph), InTasks, InSwapchain,
			                                                              {64, 64}, false, true);
			                              HYP_CHECK(std::abs(Image.Rgba.front() - InExpected) < .01f);
		                              }));
	};
	Render({{FMaterialSemanticId("Game.Amount"), FMaterialValue::Float(.25f)}}, .25f);
	Render({{FMaterialSemanticId("Game.AmountAlias"), FMaterialValue::Float(.5f)}}, .5f);
	Description.Parameters = {{FMaterialSemanticId("Game.Amount"), FMaterialValue::Float(.25f)},
	                          {FMaterialSemanticId("Game.AmountAlias"), FMaterialValue::Float(.5f)}};
	bool bRejected{};
	try
	{
		InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
		                              [&]
		                              {
			                              InSession.GetResources().GetPreparation().BuildFullscreen(Description);
		                              }));
	}
	catch (const std::invalid_argument& Error)
	{
		bRejected = std::string_view(Error.what()).find("Duplicate") != std::string_view::npos;
	}
	HYP_CHECK(bRejected);
	Render({{FMaterialSemanticId("Game.AmountAlias"), FMaterialValue::Float(.75f)}}, .75f);
}

void CheckComputeContractRejection(FTaskSystem& InTasks, FRenderSession& InSession,
                                   const std::shared_ptr<const void>& InScope)
{
	FComputePassDesc Bad;
	Bad.Shader.Source = "BadEngineCompute.hlsl";
	Bad.Lifetime = InScope;
	bool bRejected = false;
	try
	{
		InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
		                              [&]
		                              {
			                              InSession.GetResources().GetPreparation().BuildCompute(Bad);
		                              }));
	}
	catch (const std::invalid_argument& Error)
	{
		bRejected = std::string_view(Error.what()).find("ABI mismatch") != std::string_view::npos;
	}
	HYP_CHECK(bRejected);
}

void CheckReadOnlyBuffer(FTaskSystem& InTasks, FRenderSession& InSession, IRHIDevice& InDevice,
                         IRHISwapchain& InSwapchain, const std::shared_ptr<const void>& InScope)
{
	std::array<float, 133> Values;
	Values.fill(.625f);
	const FMaterialBufferView View{std::make_shared<const FMaterialReadBufferSource>(std::as_bytes(std::span(Values))),
	                               EMaterialBufferViewKind::Structured, 0, sizeof(Values), 4};
	Values.fill(99.f); // Published source bytes must not alias the producer's memory.
	const auto Output = std::make_shared<const FMaterialTextureSource>(FMaterialStorageTexture{19, 7});
	FComputePassDesc Pass;
	Pass.Name = "Read-only buffer producer";
	Pass.Shader.Source = "ReadOnlyBufferInput.hlsl";
	Pass.Lifetime = InScope;
	Pass.Extent = {19, 7, 1};
	Pass.Textures = {{"Output", Output, 0, 1, EResourceState::ShaderWrite, true, false}};
	Pass.Buffers = {{"Input", View, EResourceState::ShaderRead}};
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              FRenderGraph Graph;
		                              AddComputePass(InSession, Graph, Pass);
		                              FFullscreenPassDesc Consumer;
		                              Consumer.Material =
		                                  MakeFullscreenMaterial("Read-only buffer consumer", "ComputeConsumer.hlsl");
		                              Consumer.Lifetime = InScope;
		                              Consumer.Viewport = {0, 0, 64, 64};
		                              Consumer.Targets = FRenderPassTargets::ColorOnly(FVec4{});
		                              Consumer.Targets.Reads = {{ERenderTargetKind::Texture, Output, InScope, false}};
		                              Consumer.Targets.BufferReads = {{View, InScope}};
		                              Consumer.Parameters = {{"Pixel:Source", FMaterialValue::FromTexture(Output)},
		                                                     {"Pixel:Values", FMaterialValue::FromBuffer(View)}};
		                              AddFullscreenPass(InSession, Graph, std::move(Consumer));
		                              const auto Image =
		                                  ExecuteGraph(std::move(Graph), InTasks, InSwapchain, {64, 64}, false, true);
		                              HYP_CHECK(std::abs(Image.Rgba[0] - .625f) < .006f);
		                              HYP_CHECK(std::abs(Image.Rgba[1] - .625f) < .006f);
	                              }));
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              const auto Buffer =
		                                  InSession.GetResources().GetPreparation().ResolveBuffer(View.Source, InScope);
		                              HYP_CHECK(Buffer.Payload->GetInfo().Usage == 24);
		                              CheckOutput(InDevice, InSession, Output, InScope, .625f);
	                              }));
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
		RendererPrivate::CheckLightingWireReadback(Tasks, Session, *Device, *Swapchain, Scope);
		CheckOptionalCompute(Tasks, Session, *Device, *Swapchain, Scope);
		CheckComputeContractRejection(Tasks, Session, Scope);
		CheckAuthoredFullscreen(Tasks, Session, Scope);
		CheckFullscreenSemanticAliases(Tasks, Session, *Swapchain, Scope);
		CheckReadOnlyBuffer(Tasks, Session, *Device, *Swapchain, Scope);
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
			Pass.EngineParameters = {{EEngineSemantic::Time, FMaterialValue::Float(Frame == 5   ? .125f
			                                                                       : Frame == 0 ? .25f
			                                                                                    : .375f)},
			                         {EEngineSemantic::Index, FMaterialValue::Uint(Frame)}};
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
				    Pass.EngineParameters[0].Value =
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
				                          const auto Buffer =
				                              Session.GetResources().GetPreparation().ResolveBuffer(View.Source, Scope);
				                          HYP_CHECK(Buffer.Payload->GetInfo().Usage == 120);
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
