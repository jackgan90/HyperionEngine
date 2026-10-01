#include "Hyperion/Renderer/LightVolumePass.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Materials/ObjectParameters.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/ShaderParameters/DeferredLightingParameters.h"
#include "RenderResourcesInternal.h"

namespace Hyperion
{
namespace
{
std::shared_ptr<const FMaterialDefinition> VolumeMaterial()
{
	static const auto Definition = []
	{
		FMaterialDescription Description;
		Description.Name = "Deferred local light";
		Description.ShaderContracts = {GetDeferredLightingShaderContracts()};
		FMaterialPass Pass;
		Pass.Vertex = {"Deferred/LocalLight.hlsl", "VSMain"};
		Pass.Pixel = {"Deferred/LocalLight.hlsl", "PSMain"};
		Pass.State.Cull = EMaterialCull::Front;
		Pass.State.bDepthClip = false;
		Pass.State.bDepthTest = false;
		Pass.State.bDepthWrite = false;
		Pass.State.bBlend = true;
		Pass.State.SourceRgb = EMaterialBlendFactor::One;
		Pass.State.DestinationRgb = EMaterialBlendFactor::One;
		Pass.State.ColorWriteMask = 7;
		Description.Passes.push_back(std::move(Pass));
		return std::make_shared<const FMaterialDefinition>(std::move(Description));
	}();
	return Definition;
}

void SetView(FMaterialInstance& InMaterial, const FLightVolumePassDesc& InPass)
{
	const auto& View = InPass.View;
	const auto Port = View.Viewport.value_or(FViewport{0, 0, float(View.Width), float(View.Height)});
	InMaterial.SetSemantic(ELocalVolumeV1Field::ViewProjection, FMaterialValue::Matrix(View.ViewProjection));
	FLocalLightV1Parameters Parameters;
	Parameters.InverseViewProjection = Inverse(View.ViewProjection);
	Parameters.Viewport = {Port.X, Port.Y, Port.Width, Port.Height};
	Parameters.DepthRange = {Port.MinDepth, 1.f / (Port.MaxDepth - Port.MinDepth)};
	Parameters.Eye = View.Eye;
	InMaterial.SetParameters(MakeShaderParameters(Parameters));
	for (std::size_t Index = 0; Index < 3; ++Index)
	{
		InMaterial.SetSemantic(std::array{EDeferredLightingSemantic::GBuffer0, EDeferredLightingSemantic::GBuffer1,
		                                  EDeferredLightingSemantic::GBuffer2}[Index],
		                       FMaterialValue::FromTexture(InPass.GBuffer[Index]));
	}
	InMaterial.SetSemantic(EDeferredLightingSemantic::SceneDepth, FMaterialValue::FromTexture(InPass.Depth));
}

void SetLight(FMaterialInstance& InMaterial, const FLocalLight& InLight)
{
	InMaterial.SetSemantic(ELocalVolumeV1Field::World, FMaterialValue::Matrix(InLight.VolumeWorld));
	InMaterial.SetSemantic(ELocalLightV1Field::Position, FMaterialValue::Float(InLight.Position));
	InMaterial.SetSemantic(ELocalLightV1Field::Radiance, FMaterialValue::Float(InLight.Radiance));
	InMaterial.SetSemantic(ELocalLightV1Field::Direction, FMaterialValue::Float(InLight.Direction));
	InMaterial.SetSemantic(ELocalLightV1Field::ConeRange,
	                       FMaterialValue::Float(FVec4{1.f / InLight.Range, InLight.InnerCos, InLight.OuterCos,
	                                                   InLight.bSpot ? 1.f : 0.f}));
}
} // namespace

FGraphicsDrawBatch FRenderResourcePreparation::BuildLightVolumes(const FLightVolumePassDesc& InPass) const
{
	HYP_PERF_SCOPE_C(Render, PrepareLightVolumes);
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::lock_guard Lock(Owner.Mutex);
	if (Owner.bClosed || !InPass.Lifetime || !InPass.ConstantLifetime)
	{
		throw std::invalid_argument("Local light pass requires live resources");
	}
	Owner.EnsureMaterialCaches();
	Owner.TrackScope(InPass.Lifetime);
	auto& Volumes = Owner.LightVolumes;
	Volumes.Initialize(Owner.Device);
	auto& Entry = Volumes.Material;
	const auto Definition = VolumeMaterial();
	if (!Entry.Program)
	{
		Entry.Program = std::make_shared<const FCompiledMaterialDefinition>(
		    CompileMaterialDefinition(Owner.Compiler, Definition, Owner.Device.GetCapabilities().ShaderFormat, {},
		                              EMaterialEngineBindingMode::Explicit));
		Entry.Instance = std::make_unique<FMaterialInstance>(Entry.Program->Interface);
	}
	FGraphicsDrawBatch Result;
	try
	{
		SetView(*Entry.Instance, InPass);
		const auto& Program = Entry.Program->GetPass();
		const auto& MaterialPass = Definition->GetPass();
		const std::vector<FVertexAttribute> Attributes{{"POSITION", 0, EVertexFormat::Float3, 0}};
		for (const auto& Light : InPass.Lights)
		{
			SetLight(*Entry.Instance, Light);
			const auto Snapshot = Entry.Instance->Freeze();
			auto Values = ResolveMaterialBindingContext(Snapshot, *Entry.Program, Program, {});
			// Numeric snapshots are temporary; use a tracked pass owner so a stationary scene also collects them.
			Values.Scopes.Set(static_cast<std::size_t>(EMaterialScope::Material),
			                  {{Snapshot->Identity, Snapshot->Revision}, InPass.ConstantLifetime});
			const auto Bindings = Owner.MaterialGpu->BindResources(Program, Values.Values, {InPass.Lifetime});
			if (!Bindings.bReady)
			{
				throw std::runtime_error("Local light sampled resources are not ready");
			}
			auto Draw = Light.bSpot ? Volumes.Cone : Volumes.Sphere;
			Draw.Pipeline = Owner.MaterialGpu->GetMaterialPipeline(
			    Program, MaterialPass, Bindings.Layout, Attributes, sizeof(FVec3), ERHIPrimitiveTopology::TriangleList,
			    InPass.Targets.GraphicsTarget(false), false, {InPass.Lifetime}, InPass.View.DepthConvention);
			Draw.Bindings = Bindings.Set;
			Draw.ConstantBindings = Owner.MaterialConstants->Bind(Program, *Snapshot->Schema, Values);
			const auto Port =
			    InPass.View.Viewport.value_or(FViewport{0, 0, float(InPass.View.Width), float(InPass.View.Height)});
			Draw.Scissor = {int(Port.X), int(Port.Y), int(Port.X + Port.Width), int(Port.Y + Port.Height)};
			Result.Commands.Draws.push_back(std::move(Draw));
		}
	}
	catch (...)
	{
		Entry = {};
		throw;
	}
	return Result;
}

void FRenderSession::AppendLightVolumes(FRenderGraph& InGraph, FLightVolumePassDesc InPass, bool bInDeferPreparation)
{
	Tasks.Require({EDomain::Render});
	InPass.ConstantLifetime = Resources.CreateScopeLifetime();
	FRenderSceneSnapshot Snapshot;
	Snapshot.View = InPass.View;
	Snapshot.Targets = InPass.Targets;
	const auto Preparation = Resources.GetPreparation();
	auto Pass = Preparation.DeclarePass(InGraph, Snapshot);
	Pass.Prepare = [Preparation, Description = std::move(InPass)]
	{
		return std::vector<FGraphicsDrawBatch>{Preparation.BuildLightVolumes(Description)};
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
} // namespace Hyperion
