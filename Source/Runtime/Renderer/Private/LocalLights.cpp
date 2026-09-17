#include "Hyperion/Renderer/LocalLights.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
FLocalLight Point(FSceneHandle InHandle, const FPublishedPointLight& InSource)
{
	FLocalLight Result;
	Result.Handle = InHandle;
	Result.Position = InSource.Position;
	Result.Radiance = SceneLightRadiance(InSource.Light.Color, InSource.Light.Intensity);
	Result.Range = InSource.Light.Range;
	const auto Radius = Result.Range;
	Result.VolumeWorld = {{Radius, 0, 0, 0, 0, Radius, 0, 0, 0, 0, Radius, 0, Result.Position.X, Result.Position.Y,
	                       Result.Position.Z, 1}};
	const FVec3 Extent{Radius, Radius, Radius};
	Result.Bounds = {Subtract(Result.Position, Extent), Add(Result.Position, Extent), true};
	return Result;
}

FLocalLight Spot(FSceneHandle InHandle, const FPublishedSpotLight& InSource)
{
	const auto& Light = InSource.Light;
	auto Result = Point(InHandle, {{Light.Color, Light.Intensity, Light.Range}, InSource.Pose.Eye, InSource.bEnabled});
	Result.bSpot = true;
	Result.Direction = InSource.Pose.Forward;
	Result.InnerCos = std::cos(Light.InnerRadians);
	Result.OuterCos = std::cos(Light.OuterRadians);
	const float Radius = Light.Range * std::tan(Light.OuterRadians);
	const auto Right = ScaleVector(InSource.Pose.Right, Radius);
	const auto Up = ScaleVector(InSource.Pose.Up, Radius);
	const auto Back = ScaleVector(InSource.Pose.Forward, -Light.Range);
	Result.VolumeWorld = {{Right.X, Right.Y, Right.Z, 0, Up.X, Up.Y, Up.Z, 0, Back.X, Back.Y, Back.Z, 0,
	                       Result.Position.X, Result.Position.Y, Result.Position.Z, 1}};
	// The finite cone contains the angular/radial intersection. Intersect its AABB with the range sphere AABB.
	const auto Cone = TransformBounds({{-1, -1, -1}, {1, 1, 0}, true}, Result.VolumeWorld);
	if (IsUsable(Cone))
	{
		Result.Bounds.Minimum = {std::max(Result.Bounds.Minimum.X, Cone.Minimum.X),
		                         std::max(Result.Bounds.Minimum.Y, Cone.Minimum.Y),
		                         std::max(Result.Bounds.Minimum.Z, Cone.Minimum.Z)};
		Result.Bounds.Maximum = {std::min(Result.Bounds.Maximum.X, Cone.Maximum.X),
		                         std::min(Result.Bounds.Maximum.Y, Cone.Maximum.Y),
		                         std::min(Result.Bounds.Maximum.Z, Cone.Maximum.Z)};
	}
	return Result;
}

bool SameBounds(const FBounds& InA, const FBounds& InB)
{
	return InA.bValid == InB.bValid && InA.Minimum.X == InB.Minimum.X && InA.Minimum.Y == InB.Minimum.Y &&
	       InA.Minimum.Z == InB.Minimum.Z && InA.Maximum.X == InB.Maximum.X && InA.Maximum.Y == InB.Maximum.Y &&
	       InA.Maximum.Z == InB.Maximum.Z;
}
} // namespace

void FLocalLightIndex::Update(const FSceneMetadata& InMetadata, FSceneVisibilityStats& OutStatistics)
{
	if (Token && Token->LogicalSceneIdentity == InMetadata.Token.LogicalSceneIdentity &&
	    Token->AttachmentEpoch == InMetadata.Token.AttachmentEpoch && LocalRevision == InMetadata.LocalLightRevision)
	{
		Token = InMetadata.Token;
		return;
	}
	HYP_PERF_SCOPE_C(Render, UpdateLocalLights);
	std::map<std::uint64_t, FLocalLight> Updated;
	for (const auto& [Handle, Source] : InMetadata.PointLights)
	{
		if (Source.bEnabled && Source.Light.Intensity > 0)
		{
			Updated.emplace(std::uint64_t(Handle.Slot) * 2 + 1, Point(Handle, Source));
		}
	}
	for (const auto& [Handle, Source] : InMetadata.SpotLights)
	{
		if (Source.bEnabled && Source.Light.Intensity > 0)
		{
			Updated.emplace(std::uint64_t(Handle.Slot) * 2 + 2, Spot(Handle, Source));
		}
	}
	for (const auto& [Id, Light] : Lights)
	{
		if (!Updated.contains(Id))
		{
			Spatial->Remove(Id);
		}
	}
	for (const auto& [Id, Light] : Updated)
	{
		const auto Previous = Lights.find(Id);
		if (Previous == Lights.end() || !SameBounds(Previous->second.Bounds, Light.Bounds))
		{
			Spatial->Set(Id, Light.Bounds);
		}
	}
	Spatial->Commit(OutStatistics);
	Lights = std::move(Updated);
	Token = InMetadata.Token;
	LocalRevision = InMetadata.LocalLightRevision;
}

std::vector<FLocalLight> FLocalLightIndex::Query(const FSceneMetadata& InMetadata, const ISceneVisibility* InVisibility,
                                                 bool bInHierarchy, FLocalLightStatistics& OutStatistics)
{
	HYP_PERF_SCOPE_C(Render, CullLocalLights);
	OutStatistics.Points = InMetadata.PointLights.size();
	OutStatistics.Spots = InMetadata.SpotLights.size();
	Update(InMetadata, OutStatistics.Spatial);
	OutStatistics.Spatial.Groups = Lights.size();
	std::vector<FLocalLight> Result;
	auto Candidates = Spatial->Query(InVisibility, bInHierarchy, OutStatistics.Spatial);
	std::sort(Candidates.begin(), Candidates.end()); // Keep accumulation order independent of BVH topology.
	for (const auto Id : Candidates)
	{
		const auto& Light = Lights.at(Id);
		if (Light.Radiance.X == 0 && Light.Radiance.Y == 0 && Light.Radiance.Z == 0)
		{
			continue;
		}
		Result.push_back(Light);
		Light.bSpot ? ++OutStatistics.VisibleSpots : ++OutStatistics.VisiblePoints;
	}
	return Result;
}
} // namespace Hyperion
