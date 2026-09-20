#include "FullscreenPass.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "RenderResourcesInternal.h"
#include <algorithm>
#include <chrono>

namespace Hyperion
{
void FFullscreenResources::Initialize(IRHIDevice& InDevice)
{
	if (Triangle.Vertices)
	{
		return;
	}
	const std::array<FVec2, 3> Positions{{{-1, -1}, {3, -1}, {-1, 3}}};
	const std::array<std::uint32_t, 3> Indices{0, 1, 2};
	FDrawPacket Prepared;
	Prepared.Vertices = InDevice.CreateBuffer(std::as_bytes(std::span(Positions)));
	Prepared.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	Prepared.VertexStride = sizeof(FVec2);
	Prepared.IndexCount = 3;
	Triangle = std::move(Prepared);
}

void FFullscreenResources::Collect()
{
	std::erase_if(Materials,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Lifetime.expired();
	              });
}

std::shared_ptr<const FMaterialDefinition> MakeFullscreenMaterial(std::string InName, std::string InPixelShader,
                                                                  bool bInSrgb)
{
	FMaterialDescription Description;
	Description.Name = std::move(InName);
	FMaterialPass Pass;
	Pass.Vertex = {"Common/Fullscreen.hlsl", "VSMain"};
	Pass.Pixel = {std::move(InPixelShader), "PSMain"};
	Pass.bSrgbTarget = bInSrgb;
	Description.Passes.push_back(std::move(Pass));
	return std::make_shared<const FMaterialDefinition>(std::move(Description));
}

namespace
{
void UpdateMaterial(FFullscreenResources::FMaterialEntry& InEntry, const FMaterialParameterValues& InValues)
{
	for (const auto& Previous : InEntry.Values)
	{
		if (std::none_of(InValues.begin(), InValues.end(),
		                 [&](const auto& InValue)
		                 {
			                 return InValue.Name == Previous.Name;
		                 }))
		{
			InEntry.Instance->Clear(Previous.Name);
		}
	}
	for (const auto& Value : InValues)
	{
		InEntry.Instance->Set(Value.Name, Value.Value);
	}
	InEntry.Values = InValues;
}

void TrackReadOwners(FRenderResourceCoordinator& InOwner, const FFullscreenPassDesc& InPass,
                     FMaterialResourceOwners& InBindingOwners)
{
	for (const auto& Read : InPass.Targets.Reads)
	{
		if (Read.Lifetime &&
		    std::find(InBindingOwners.begin(), InBindingOwners.end(), Read.Lifetime) == InBindingOwners.end())
		{
			InOwner.TrackScope(Read.Lifetime);
			InBindingOwners.push_back(Read.Lifetime);
		}
	}
	for (const auto& Read : InPass.Targets.BufferReads)
	{
		if (Read.Lifetime &&
		    std::find(InBindingOwners.begin(), InBindingOwners.end(), Read.Lifetime) == InBindingOwners.end())
		{
			InOwner.TrackScope(Read.Lifetime);
			InBindingOwners.push_back(Read.Lifetime);
		}
	}
}
} // namespace

FGraphicsDrawBatch FRenderResourcePreparation::BuildFullscreen(const FFullscreenPassDesc& InPass) const
{
	const auto Start = InPass.Statistics ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::lock_guard Lock(Owner.Mutex);
	if (Owner.bClosed || !InPass.Material || !InPass.Lifetime)
	{
		throw std::invalid_argument("Fullscreen pass requires a material and live resource scope");
	}
	Owner.EnsureMaterialCaches();
	Owner.TrackScope(InPass.Lifetime);
	auto& Fullscreen = Owner.Fullscreen;
	Fullscreen.Initialize(Owner.Device);
	auto& Entry = Fullscreen.Materials[InPass.Material->GetIdentity()];
	if (!Entry.Program)
	{
		auto Program = std::make_shared<const FCompiledMaterialDefinition>(
		    CompileMaterialDefinition(Owner.Compiler, InPass.Material, Owner.Device.GetCapabilities().ShaderFormat));
		auto Instance = std::make_unique<FMaterialInstance>(Program->Interface);
		Entry.Program = std::move(Program);
		Entry.Instance = std::move(Instance);
	}
	Entry.Lifetime = InPass.Lifetime;
	try
	{
		UpdateMaterial(Entry, InPass.Parameters);
	}
	catch (...)
	{
		// Set/Clear publish individually; discard any partially updated instance.
		Fullscreen.Materials.erase(InPass.Material->GetIdentity());
		throw;
	}
	const auto Snapshot = Entry.Instance->Freeze();
	const auto& Program = Entry.Program->GetPass();
	auto Values = ResolveMaterialBindingContext(Snapshot, *Entry.Program, Program, {});
	FMaterialResourceOwners BindingOwners{InPass.Lifetime};
	if (InPass.ResourceLifetime)
	{
		Owner.TrackScope(InPass.ResourceLifetime);
		BindingOwners.push_back(InPass.ResourceLifetime);
	}
	if (InPass.ParameterLifetime)
	{
		Owner.TrackScope(InPass.ParameterLifetime);
		BindingOwners.push_back(InPass.ParameterLifetime);
		Values.Scopes.Set(static_cast<std::size_t>(EMaterialScope::Material),
		                  {{Snapshot->Identity, Snapshot->Revision}, InPass.ConstantLifetime});
	}
	TrackReadOwners(Owner, InPass, BindingOwners);
	const auto Bindings = Owner.MaterialGpu->BindResources(Program, Values.Values, BindingOwners);
	if (!Bindings.bReady)
	{
		// Upload completion is asynchronous, including first-use neutral environment inputs.
		// Preserve the pass attachments and retry preparation next frame without a GPU wait.
		FGraphicsDrawBatch Pending;
		Pending.bSrgb = InPass.Material->GetPass().bSrgbTarget;
		return Pending;
	}
	const auto& MaterialPass = InPass.Material->GetPass();
	const auto Target = InPass.Targets.GraphicsTarget(MaterialPass.bSrgbTarget);
	const std::vector<FVertexAttribute> Attributes{{"POSITION", 0, EVertexFormat::Float2, 0}};
	auto Draw = Fullscreen.Triangle;
	Draw.DynamicState = ConvertMaterialDynamicState(MaterialPass.DynamicState);
	ValidateGraphicsDynamicState(Draw.DynamicState);
	Draw.Pipeline = Owner.MaterialGpu->GetMaterialPipeline(Program, MaterialPass, Bindings.Layout, Attributes,
	                                                       sizeof(FVec2), ERHIPrimitiveTopology::TriangleList, Target,
	                                                       false, {InPass.Lifetime}, InPass.DepthConvention);
	Draw.Bindings = Bindings.Set;
	Draw.ConstantBindings = Owner.MaterialConstants->Bind(Program, *Snapshot->Schema, Values);
	const auto& View = InPass.Viewport;
	Draw.Scissor =
	    InPass.Scissor.value_or(FRect{static_cast<int>(View.X), static_cast<int>(View.Y),
	                                  static_cast<int>(View.X + View.Width), static_cast<int>(View.Y + View.Height)});
	FGraphicsDrawBatch Result;
	Result.bSrgb = MaterialPass.bSrgbTarget;
	Result.Commands.Draws.push_back(std::move(Draw));
	if (InPass.Statistics)
	{
		InPass.Statistics->Milliseconds +=
		    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
		++InPass.Statistics->Draws;
	}
	return Result;
}

void FRenderSession::AppendFullscreen(FRenderGraph& InGraph, FFullscreenPassDesc InPass, bool bInDeferPreparation)
{
	Tasks.Require({EDomain::Render});
	if (InPass.ParameterLifetime)
	{
		InPass.ConstantLifetime = Resources.CreateScopeLifetime();
	}
	FRenderSceneSnapshot Snapshot;
	if (!InPass.bFullTargetViewport)
	{
		Snapshot.View.Viewport = InPass.Viewport;
	}
	Snapshot.Targets = InPass.Targets;
	const auto Preparation = Resources.GetPreparation();
	auto Pass = Preparation.DeclarePass(InGraph, Snapshot);
	Pass.Prepare = [Preparation, Description = std::move(InPass)]
	{
		return std::vector<FGraphicsDrawBatch>{Preparation.BuildFullscreen(Description)};
	};
	if (!bInDeferPreparation)
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Pass.Batches = Pass.Prepare();
		                          }));
		Pass.Prepare = {};
	}
	InGraph.Add(std::move(Pass));
}

void AddFullscreenPass(FRenderSession& InSession, FRenderGraph& InGraph, FFullscreenPassDesc InPass,
                       bool bInDeferPreparation)
{
	InSession.AppendFullscreen(InGraph, std::move(InPass), bInDeferPreparation);
}
} // namespace Hyperion
