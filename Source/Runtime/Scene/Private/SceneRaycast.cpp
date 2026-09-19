#include "SceneQueryInternal.h"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace Hyperion
{
namespace
{
std::shared_ptr<const FMaterialSnapshot> SelectedMaterial(const FSceneMaterialSelection& InSelection)
{
	return InSelection.Instance ? InSelection.Instance->Freeze() : InSelection.Snapshot;
}

std::optional<FMaterialState> QueryMaterial(const FSceneModel& InModel, std::uint32_t InPrimitive,
                                            const FSceneRayOptions& InOptions)
{
	if (InOptions.bTwoSided)
	{
		return FMaterialState{};
	}
	auto Snapshot = SelectedMaterial(InModel.Surface);
	const auto Section = InModel.SectionSurfaces.find(InPrimitive);
	if (Section != InModel.SectionSurfaces.end())
	{
		if (auto Override = SelectedMaterial(Section->second))
		{
			Snapshot = std::move(Override);
		}
	}
	const auto Material = InModel.Data->Asset->Primitives[InPrimitive].Material;
	if (!Snapshot && Material >= 0 && std::size_t(Material) < InModel.Data->MaterialSnapshots.size())
	{
		Snapshot = InModel.Data->MaterialSnapshots[Material];
	}
	if (!Snapshot || !Snapshot->Definition)
	{
		return FMaterialState{};
	}
	for (const auto& Usage : InOptions.MaterialUsages)
	{
		const auto Exclusions = InOptions.MaterialUsageExclusions.find(Usage);
		const bool bExcluded = Exclusions != InOptions.MaterialUsageExclusions.end() &&
		                       std::any_of(Exclusions->second.begin(), Exclusions->second.end(),
		                                   [&](const std::string& InExcluded)
		                                   {
			                                   return Snapshot->Definition->HasPass(InExcluded);
		                                   });
		if (!bExcluded && Snapshot->Definition->HasPass(Usage))
		{
			return Snapshot->Definition->GetPass(Usage).State;
		}
	}
	return {};
}

FVec3 Vertex(const FModelPrimitive& InPrimitive, std::uint32_t InTriangle, unsigned InCorner)
{
	const auto Index = std::size_t(InPrimitive.Indices[std::size_t(InTriangle) * 3 + InCorner]) * 3;
	return {InPrimitive.Positions[Index], InPrimitive.Positions[Index + 1], InPrimitive.Positions[Index + 2]};
}

struct FModelRaycast
{
	const FSceneModel& Model;
	FSceneHandle Handle;
	FRay WorldRay;
	const FSceneRayOptions& Options;
	FSceneRayResult& Result;
	void CastInstance(const FModelInstance& InInstance, std::uint32_t InIndex);
	void Accept(std::uint32_t InInstance, std::uint32_t InPrimitive, std::uint32_t InTriangle,
	            const FRayTriangleHit& InHit, const FMaterialState& InMaterial, float& OutMaximum);
};

void FModelRaycast::Accept(std::uint32_t InInstance, std::uint32_t InPrimitive, std::uint32_t InTriangle,
                           const FRayTriangleHit& InHit, const FMaterialState& InMaterial, float& OutMaximum)
{
	const bool bFront = InHit.bFrontFacing == InMaterial.bFrontCounterClockwise;
	if ((bFront && InMaterial.Cull == EMaterialCull::Front) || (!bFront && InMaterial.Cull == EMaterialCull::Back))
	{
		return;
	}
	if (Result.Status == ESceneRayStatus::Hit &&
	    (InHit.Distance > Result.Distance ||
	     (InHit.Distance == Result.Distance &&
	      std::tie(Handle, InInstance, InPrimitive, InTriangle) >=
	          std::tie(Result.Handle, Result.Instance, Result.Primitive, Result.Triangle))))
	{
		return;
	}
	Result.Status = ESceneRayStatus::Hit;
	Result.Handle = Handle;
	Result.Instance = InInstance;
	Result.Primitive = InPrimitive;
	Result.Triangle = InTriangle;
	Result.Distance = InHit.Distance;
	Result.Position = Add(WorldRay.Origin, ScaleVector(WorldRay.Direction, InHit.Distance));
	Result.Barycentrics = InHit.Barycentrics;
	OutMaximum = SceneRayMaximum(Result, OutMaximum);
}

void FModelRaycast::CastInstance(const FModelInstance& InInstance, std::uint32_t InIndex)
{
	const auto PrimitiveId = ModelPrimitiveId(*Model.Data->Asset, InInstance.Primitive);
	const auto Section = std::find_if(Model.Sections.begin(), Model.Sections.end(),
	                                  [&](const auto& InSection)
	                                  {
		                                  return InSection.Primitive == PrimitiveId;
	                                  });
	if (Section != Model.Sections.end() && !Section->bVisible)
	{
		return;
	}
	++Result.Stats.InstanceTests;
	const auto World = Multiply(Model.World, InInstance.World);
	FRay Ray = WorldRay;
	Ray.Maximum = SceneRayMaximum(Result, Ray.Maximum);
	if (!IntersectRayBounds(Ray, TransformBounds(Model.Data->PrimitiveBounds[InInstance.Primitive], World)))
	{
		return;
	}
	bool bLocal{};
	try
	{
		Ray = TransformRay(Ray, Inverse(World));
		bLocal = IsUsable(Ray);
	}
	catch (const std::invalid_argument&)
	{
	}
	if (!bLocal)
	{
		Ray = WorldRay;
		Ray.Maximum = SceneRayMaximum(Result, Ray.Maximum);
	}
	const auto SelectedState = QueryMaterial(Model, InInstance.Primitive, Options);
	if (!SelectedState)
	{
		return;
	}
	auto Material = *SelectedState;
	if (!bLocal && Determinant(World) < 0)
	{
		Material.bFrontCounterClockwise = !Material.bFrontCounterClockwise;
	}
	const auto& Primitive = Model.Data->Asset->Primitives[InInstance.Primitive];
	const auto Visit = [&](std::uint32_t InTriangle, float& OutMaximum)
	{
		++Result.Stats.TriangleTests;
		auto A = Vertex(Primitive, InTriangle, 0);
		auto B = Vertex(Primitive, InTriangle, 1);
		auto C = Vertex(Primitive, InTriangle, 2);
		if (!bLocal)
		{
			const auto Point = [&](FVec3 InPoint)
			{
				const auto P = Transform(World, {InPoint.X, InPoint.Y, InPoint.Z, 1});
				return FVec3{P.X, P.Y, P.Z};
			};
			A = Point(A);
			B = Point(B);
			C = Point(C);
		}
		FRay Limited = Ray;
		Limited.Maximum = OutMaximum;
		if (const auto Hit = IntersectRayTriangle(Limited, A, B, C))
		{
			Accept(InIndex, InInstance.Primitive, InTriangle, *Hit, Material, OutMaximum);
		}
	};
	if (bLocal)
	{
		Model.Data->QueryGeometry->Primitives[InInstance.Primitive].Raycast(Ray, Visit, Result.Stats.TriangleNodes);
	}
	else
	{
		for (std::uint32_t Triangle = 0; Triangle < Primitive.Indices.size() / 3; ++Triangle)
		{
			Visit(Triangle, Ray.Maximum);
		}
	}
}
} // namespace

void RaycastSceneModel(const FSceneModel& InModel, FSceneHandle InHandle, FRay InRay, const FSceneRayOptions& InOptions,
                       FSceneRayResult& OutResult)
{
	if (!InModel.Data || !InModel.Data->QueryGeometry)
	{
		++OutResult.Stats.UnavailableCandidates;
		return;
	}
	FModelRaycast Query{InModel, InHandle, InRay, InOptions, OutResult};
	const auto Instances = SceneModelInstances(InModel);
	for (std::uint32_t Index = 0; Index < Instances.size(); ++Index)
	{
		Query.CastInstance(Instances[Index], Index);
	}
}
} // namespace Hyperion
