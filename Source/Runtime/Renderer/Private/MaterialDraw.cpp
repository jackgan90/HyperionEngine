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
			    !SameMaterialValue(InValues[Index++], InParameters.Values.at(*Binding.ResourceParameter)))
			{
				return false;
			}
		}
	}
	return Index == InValues.size();
}

FMaterialResourceOwners ResourceOwners(const std::shared_ptr<const void>& InLifetime,
                                       const FCompiledMaterialPass& InProgram,
                                       const FResolvedMaterialParameters& InValues)
{
	FMaterialResourceOwners Owners{InLifetime};
	std::uint32_t Dependencies{};
	for (const auto& Binding : InProgram.Bindings)
	{
		if (Binding.ResourceParameter)
		{
			Dependencies |= InValues.Dependencies[*Binding.ResourceParameter];
		}
	}
	// Numeric Object/View changes never invalidate static descriptor tables.
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if ((Dependencies & (1U << Scope)) && Scope != static_cast<std::size_t>(EMaterialScope::Material))
		{
			Owners.push_back(InValues.Scopes[Scope].Lifetime);
		}
	}
	return Owners;
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
                                                     FGraphicsTarget InTarget)
{
	const auto& GeometryRecord = *InItem.State.Resource->Record;
	const auto& MaterialRecord = *InItem.State.Surface->Record;
	if (GeometryRecord.Owner != this || MaterialRecord.Owner != this ||
	    GeometryRecord.Status != ERenderResourceStatus::Ready || MaterialRecord.Status != ERenderMaterialStatus::Ready)
	{
		throw std::invalid_argument("Unready or foreign geometry/material resource");
	}
	const auto& Compiled = *MaterialRecord.Compiled;
	const auto& Program = Compiled.GetPass(InView.Usage);
	const auto& Snapshot = InItem.State.Surface->GetSnapshot();
	const auto& Pass = Snapshot->Definition->GetPass(InView.Usage);
	const auto Resolved = InItem.ResolvedParameters
	                          ? InItem.ResolvedParameters
	                          : std::make_shared<const FResolvedMaterialParameters>(
	                                ResolveMaterialBindingContext(Snapshot, Compiled, Program, InItem.Context));
	const bool bMirrored = Determinant(InItem.State.World) < 0;
	const std::shared_ptr<const void> ResourceIdentity =
	    Resolved->ResourceIdentity ? Resolved->ResourceIdentity : Resolved;
	const FDrawKey Key{ResourceIdentity.get(), InItem.State.Resource.get(), InItem.State.Section, InView.Usage};
	const auto Existing = PreparedDraws.find(Key);
	if (Existing != PreparedDraws.end())
	{
		auto& Cached = Existing->second;
		const bool bSameParameters = Cached.Parameters.lock() == Resolved;
		if (Cached.Resources.lock() == ResourceIdentity && Cached.Geometry.lock() == InItem.State.Resource &&
		    Cached.Surface.lock() == InItem.State.Surface && Cached.Target == InTarget &&
		    Cached.bMirrored == bMirrored &&
		    (bSameParameters || SameResources(Cached.ResourceValues, Program, *Resolved)))
		{
			if (!bSameParameters)
			{
				Cached.Packet.ConstantBindings =
				    MaterialConstants->Bind(Program, *Compiled.Interface.Schema, *Resolved);
				Cached.Parameters = Resolved;
			}
			return FinalizeDraw(Cached.Packet, InItem, InView, Pass);
		}
	}
	const auto& Values = *Resolved;
	const auto Owners = ResourceOwners(MaterialRecord.GpuLifetime, Program, Values);
	EnsureMaterialCaches();
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
	    Determinant(InItem.State.World) < 0, {MaterialRecord.GpuLifetime, InItem.State.Resource});
	Result.Vertices = GeometryRecord.Vertices[Section.Geometry];
	Result.Indices = GeometryRecord.Indices[Section.Geometry];
	Result.VertexStride = Geometry.VertexStride;
	Result.FirstIndex = Section.FirstIndex;
	Result.IndexCount = Section.IndexCount;
	Result.Bindings = Bindings.Set;
	Result.ConstantBindings = MaterialConstants->Bind(Program, *Compiled.Interface.Schema, Values);
	Result = FinalizeDraw(std::move(Result), InItem, InView, Pass);
	std::vector<std::shared_ptr<const FMaterialValue>> ResourceValues;
	for (const auto& Binding : Program.Bindings)
	{
		if (Binding.ResourceParameter)
		{
			ResourceValues.push_back(Resolved->Values.at(*Binding.ResourceParameter));
		}
	}
	PreparedDraws.insert_or_assign(Key, FPreparedDraw{Resolved, ResourceIdentity, std::move(ResourceValues),
	                                                  InItem.State.Resource, InItem.State.Surface, InTarget, bMirrored,
	                                                  Result});
	return Result;
}
} // namespace Hyperion
