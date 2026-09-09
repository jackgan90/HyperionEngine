#include "Hyperion/Renderer/RenderPrimitive.h"
#include "Hyperion/Renderer/RenderResources.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace Hyperion
{
IRenderPrimitive::IRenderPrimitive(FTaskSystem& InTasks) : Tasks(InTasks)
{
	Tasks.Require({EDomain::Render});
}

IRenderPrimitive::~IRenderPrimitive()
{
	if (!Tasks.IsCurrent({EDomain::Render}))
	{
		std::terminate();
	}
}

void IRenderPrimitive::Apply(FRenderPrimitiveState InState) noexcept
{
	if (!Tasks.IsCurrent({EDomain::Render}))
	{
		std::terminate();
	}
	State = std::move(InState);
}

const FRenderPrimitiveState& IRenderPrimitive::GetState() const
{
	Tasks.Require({EDomain::Render});
	return State;
}

FBounds IRenderPrimitive::GetWorldBounds() const
{
	Tasks.Require({EDomain::Render});
	return {};
}

FBounds FStaticMeshRenderPrimitive::GetWorldBounds() const
{
	const auto& Current = GetState();
	if (Current.bClipSpace)
	{
		return {};
	}
	auto Surface = Current.Surface;
	if (!Surface && Current.Resource)
	{
		Surface = Current.Resource->GetMaterial(Current.Section);
	}
	if (Surface && !Current.bConservativeBounds)
	{
		const auto& Passes = Surface->GetSnapshot()->Definition->GetDescription().Passes;
		if (std::any_of(Passes.begin(), Passes.end(),
		                [](const auto& InPass)
		                {
			                return InPass.bRequiresConservativeBounds;
		                }))
		{
			return {};
		}
	}
	if (!Current.Resource)
	{
		return TransformBounds(Current.LocalBounds, Current.World);
	}
	const auto Desc = Current.Resource ? Current.Resource->GetDescription() : nullptr;
	if (!Desc || Current.Section >= Desc->Sections.size())
	{
		return {};
	}
	const auto& Section = Desc->Sections[Current.Section];
	return TransformBounds(
	    IsUsable(Current.LocalBounds) ? Current.LocalBounds : Desc->Geometries[Section.Geometry].Bounds, Current.World);
}

void FStaticMeshRenderPrimitive::Collect(const FRenderView&, std::vector<FRenderItem>& OutItems) const
{
	const auto& PrimitiveState = GetState();
	if (PrimitiveState.bVisible)
	{
		auto& Item = OutItems.emplace_back();
		Item.State = PrimitiveState;
		Item.LocalItemId = 0;
	}
}

void ValidatePrimitiveState(const FRenderPrimitiveState& InState)
{
	if (InState.Resource)
	{
		const auto Description = InState.Resource->GetDescription();
		if (Description && InState.Section >= Description->Sections.size())
		{
			throw std::invalid_argument("Invalid primitive section");
		}
	}
	auto Surface = InState.Surface;
	if (!Surface && InState.Resource)
	{
		Surface = InState.Resource->GetMaterial(InState.Section);
	}
	if (Surface && Surface->GetCompiled())
	{
		auto Snapshot = *Surface->GetSnapshot();
		Snapshot.Schema = Surface->GetCompiled()->Interface.Schema;
		const std::array<std::size_t, 0> NoRequiredParameters{};
		ResolveMaterialParameters(Snapshot, {}, GetPrimitiveMaterialOverrides(InState, *Snapshot.Schema), {},
		                          NoRequiredParameters);
	}
	else
	{
		for (const auto* Level : {&InState.ObjectParameters, &InState.SectionParameters})
		{
			for (const auto& Override : *Level)
			{
				Override.Value.Validate();
			}
		}
	}
	if (!InState.Revision || !std::all_of(InState.World.Values.begin(), InState.World.Values.end(),
	                                      [](float InValue)
	                                      {
		                                      return std::isfinite(InValue);
	                                      }))
	{
		throw std::invalid_argument("Invalid primitive revision or transform");
	}
	if (InState.Material.BaseColor)
	{
		const auto Color = *InState.Material.BaseColor;
		if (!std::isfinite(Color.X) || !std::isfinite(Color.Y) || !std::isfinite(Color.Z) || !std::isfinite(Color.W))
		{
			throw std::invalid_argument("Invalid primitive material color");
		}
	}
	for (const auto Value : {InState.Material.Metallic, InState.Material.Roughness})
	{
		if (Value && (!std::isfinite(*Value) || *Value < 0 || *Value > 1))
		{
			throw std::invalid_argument("Invalid primitive material parameter");
		}
	}
}
} // namespace Hyperion
