#include "Hyperion/Core/Profiling.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "RenderResourcesInternal.h"
#include <stdexcept>

namespace Hyperion
{
namespace
{
bool SameResources(const std::vector<std::shared_ptr<const FMaterialValue>>& InValues,
                   const FCompiledMaterialPass& InPass, const FResolvedMaterialParameters& InParameters)
{
	std::size_t Index{};
	for (const auto& Binding : InPass.Bindings)
	{
		if (Binding.ResourceParameter)
		{
			if (Index >= InValues.size() ||
			    !SameMaterialValue(InValues[Index++], InParameters.Values.Get(*Binding.ResourceParameter)))
			{
				return false;
			}
		}
	}
	return Index == InValues.size();
}

FDrawPacket FinalizeDraw(FDrawPacket InDraw, const FRenderItem& InItem, const FRenderView& InView,
                         const FMaterialPass& InPass)
{
	if (InItem.DynamicState && !InPass.bAllowDynamicOverrides)
	{
		throw std::invalid_argument("Material pass does not allow dynamic draw-state overrides");
	}
	InDraw.DynamicState = ConvertMaterialDynamicState(InItem.DynamicState.value_or(InPass.DynamicState));
	ValidateGraphicsDynamicState(InDraw.DynamicState);
	const auto Viewport = InView.Viewport.value_or(FViewport{0, 0, float(InView.Width), float(InView.Height)});
	InDraw.Scissor = {static_cast<std::int32_t>(Viewport.X), static_cast<std::int32_t>(Viewport.Y),
	                  static_cast<std::int32_t>(Viewport.X + Viewport.Width),
	                  static_cast<std::int32_t>(Viewport.Y + Viewport.Height)};
	return InDraw;
}
} // namespace

void FRenderResourceCoordinator::CollectPreparedDraws()
{
	std::erase_if(PreparedDraws,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Resources.expired() || InEntry.second.Geometry.expired() ||
		                     InEntry.second.Surface.expired();
	              });
}

FDrawPacket FRenderResourceCoordinator::DrawMaterial(const FRenderItem& InItem, const FRenderView& InView,
                                                     FGraphicsTarget InTarget, bool bInInstance)
{
	HYP_PERF_SCOPE_C(Detail, DrawMaterial);
	const auto& GeometryRecord = *InItem.State.Resource->Record;
	const auto& MaterialRecord = *InItem.State.Surface->Record;
	if (GeometryRecord.Owner != this || MaterialRecord.Owner != this ||
	    GeometryRecord.Status != ERenderResourceStatus::Ready || MaterialRecord.Status != ERenderMaterialStatus::Ready)
	{
		throw std::invalid_argument("Unready or foreign geometry/material resource");
	}
	const auto& Compiled = *MaterialRecord.Compiled;
	const auto& Program = Compiled.GetPass(InView.Usage, bInInstance ? "Instance" : "Default");
	const auto& Snapshot = InItem.State.Surface->GetSnapshot();
	const auto& Pass = Snapshot->Definition->GetPass(InView.Usage);
	const auto Resolved = InItem.ResolvedParameters
	                          ? InItem.ResolvedParameters
	                          : std::make_shared<const FResolvedMaterialParameters>(
	                                ResolveMaterialBindingContext(Snapshot, Compiled, Program, InItem.Context));
	const auto Shared = InItem.SharedParameters
	                        ? std::optional(ComposeMaterialParameters(*Resolved, *InItem.SharedParameters))
	                        : std::nullopt;
	const auto& Values = Shared ? *Shared : *Resolved;
	const bool bMirrored = Determinant(InItem.State.World) < 0;
	const std::shared_ptr<const void> ResourceIdentity =
	    Resolved->ResourceIdentity ? Resolved->ResourceIdentity : Resolved;
	const FDrawKey Key{ResourceIdentity.get(), InItem.State.Resource.get(), InItem.State.Section, InView.Usage,
	                   bInInstance};
	const auto Existing = PreparedDraws.find(Key);
	if (Existing != PreparedDraws.end())
	{
		auto& Cached = Existing->second;
		const bool bSameParameters =
		    Cached.Parameters.lock() == Resolved && Cached.Shared.lock() == InItem.SharedParameters;
		if (Cached.Resources.lock() == ResourceIdentity && Cached.Geometry.lock() == InItem.State.Resource &&
		    Cached.Surface.lock() == InItem.State.Surface && Cached.Target == InTarget &&
		    Cached.bMirrored == bMirrored && Cached.DepthConvention == InView.DepthConvention &&
		    (bSameParameters || SameResources(Cached.ResourceValues, Program, Values)))
		{
			if (!bSameParameters)
			{
				Cached.Packet.ConstantBindings = MaterialConstants->BindPrepared(MaterialRecord.Compiled, Program,
				                                                                 Values, Cached.Constants, bInInstance);
				Cached.Parameters = Resolved;
				Cached.Shared = InItem.SharedParameters;
			}
			return FinalizeDraw(Cached.Packet, InItem, InView, Pass);
		}
	}
	// The resource-value epoch survives numeric scope changes, and expires when its last evaluation retires.
	// Keeping an obsolete View owner here would turn a live cached set into perpetual pending retirement.
	const FMaterialResourceOwners Owners{MaterialRecord.GpuLifetime, ResourceIdentity};
	EnsureMaterialCaches();
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if (Values.DependenciesMask & (1U << Scope))
		{
			TrackScope(Values.Scopes[Scope].Lifetime);
		}
	}
	const auto Bindings = MaterialGpu->BindResources(Program, Values.Values, Owners);
	if (!Bindings.bReady)
	{
		throw std::runtime_error("Material resource upload is pending in this draw context");
	}
	const auto& Section = GeometryRecord.Description->Sections.at(InItem.State.Section);
	const auto& Geometry = GeometryRecord.Description->Geometries[Section.Geometry];
	FDrawPacket Result;
	Result.Pipeline = MaterialGpu->GetMaterialPipeline(
	    Program, Pass, Bindings.Layout, Geometry.Attributes, Geometry.VertexStride, Geometry.Topology, InTarget,
	    bMirrored, {MaterialRecord.GpuLifetime, InItem.State.Resource}, InView.DepthConvention);
	Result.Vertices = GeometryRecord.Vertices[Section.Geometry];
	Result.Indices = GeometryRecord.Indices[Section.Geometry];
	Result.VertexStride = Geometry.VertexStride;
	Result.FirstIndex = Section.FirstIndex;
	Result.IndexCount = Section.IndexCount;
	Result.Bindings = Bindings.Set;
	FMaterialConstantState Constants;
	Result.ConstantBindings =
	    MaterialConstants->BindPrepared(MaterialRecord.Compiled, Program, Values, Constants, bInInstance);
	Result = FinalizeDraw(std::move(Result), InItem, InView, Pass);
	std::vector<std::shared_ptr<const FMaterialValue>> ResourceValues;
	for (const auto& Binding : Program.Bindings)
	{
		if (Binding.ResourceParameter)
		{
			ResourceValues.push_back(Values.Values.Get(*Binding.ResourceParameter));
		}
	}
	PreparedDraws.insert_or_assign(Key, FPreparedDraw{Resolved, ResourceIdentity, std::move(ResourceValues),
	                                                  InItem.State.Resource, InItem.State.Surface, InTarget, bMirrored,
	                                                  Result, std::move(Constants), InItem.SharedParameters,
	                                                  InView.DepthConvention});
	return Result;
}
} // namespace Hyperion
