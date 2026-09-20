#include "Hyperion/Renderer/ViewportPlacement.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
bool FViewportPlacementContext::operator==(const FViewportPlacementContext& InOther) const
{
	return Document == InOther.Document && Revision == InOther.Revision && Bounds.X == InOther.Bounds.X &&
	       Bounds.Y == InOther.Bounds.Y && Bounds.Z == InOther.Bounds.Z && Bounds.W == InOther.Bounds.W &&
	       PixelScale.X == InOther.PixelScale.X && PixelScale.Y == InOther.PixelScale.Y &&
	       ApplicationScale == InOther.ApplicationScale && Camera.Lens == InOther.Camera.Lens &&
	       Camera.World.Values == InOther.Camera.World.Values;
}

void FViewportPlacementSession::Begin(std::string InType, FViewportPlacementContext InContext)
{
	if (InType.empty())
	{
		throw std::invalid_argument("Placement requires a stable object type");
	}
	Type = std::move(InType);
	Context = std::move(InContext);
	Preview.reset();
}

bool FViewportPlacementSession::IsCurrent(std::string_view InType, const FViewportPlacementContext& InContext) const
{
	return IsActive() && Type == InType && Context == InContext;
}

void FViewportPlacementSession::SetPreview(std::optional<FViewportPlacement> InPreview)
{
	Preview = IsActive() ? std::move(InPreview) : std::nullopt;
}

void FViewportPlacementSession::Cancel()
{
	Type.clear();
	Preview.reset();
}

bool FViewportPlacementSession::IsActive() const
{
	return !Type.empty();
}

const std::string& FViewportPlacementSession::GetType() const
{
	return Type;
}

const std::optional<FViewportPlacement>& FViewportPlacementSession::GetPreview() const
{
	return Preview;
}

namespace
{
std::optional<FVec3> OnPlane(const FRay& InRay, FVec3 InPoint, FVec3 InNormal)
{
	const float Denominator = Dot(InRay.Direction, InNormal);
	if (std::abs(Denominator) < .00001f)
	{
		return {};
	}
	const float Distance = Dot(Subtract(InPoint, InRay.Origin), InNormal) / Denominator;
	if (!std::isfinite(Distance) || Distance < InRay.Minimum || Distance > InRay.Maximum)
	{
		return {};
	}
	return Add(InRay.Origin, ScaleVector(InRay.Direction, Distance));
}
} // namespace

std::optional<FViewportPlacement> ResolveViewportPlacement(const FRay& InRay, const FSceneRayResult& InHit,
                                                           const FSceneCameraView& InCamera,
                                                           const FBounds& InLocalBounds)
{
	if (!IsUsable(InRay) || InHit.Status == ESceneRayStatus::Unavailable || InHit.bIncomplete)
	{
		return {};
	}
	FViewportPlacement Result;
	if (InHit.Status == ESceneRayStatus::Hit)
	{
		Result.Position = InHit.Position;
		Result.Normal = Dot(InHit.Normal, InRay.Direction) > 0 ? ScaleVector(InHit.Normal, -1) : InHit.Normal;
		Result.bSurface = true;
	}
	else if (const auto Ground = OnPlane(InRay, {}, {0, 1, 0}))
	{
		Result.Position = *Ground;
	}
	else
	{
		const auto Pose = ExtractScenePose(InCamera.World);
		const float Distance = std::clamp(InCamera.Lens.FocusDistance, InCamera.Lens.Near, InCamera.Lens.Far);
		const auto Point = OnPlane(InRay, Add(Pose.Eye, ScaleVector(Pose.Forward, Distance)), Pose.Forward);
		if (!Point)
		{
			return {};
		}
		Result.Position = *Point;
		return Result;
	}
	if (IsUsable(InLocalBounds))
	{
		const auto& N = Result.Normal;
		const FVec3 Support{N.X >= 0 ? InLocalBounds.Minimum.X : InLocalBounds.Maximum.X,
		                    N.Y >= 0 ? InLocalBounds.Minimum.Y : InLocalBounds.Maximum.Y,
		                    N.Z >= 0 ? InLocalBounds.Minimum.Z : InLocalBounds.Maximum.Z};
		Result.Position = Add(Result.Position, ScaleVector(N, std::max(0.f, -Dot(Support, N)) + .002f));
	}
	return Result;
}

std::optional<FVec3> ProjectViewportPoint(const FSceneCameraView& InCamera, FVec4 InBounds, FVec3 InWorld)
{
	const float Width = InBounds.Z - InBounds.X;
	const float Height = InBounds.W - InBounds.Y;
	if (Width <= 0 || Height <= 0)
	{
		return {};
	}
	const auto Matrix = SceneCameraViewProjection(ExtractScenePose(InCamera.World), InCamera.Lens, Width / Height,
	                                              EDepthConvention::Standard);
	const auto Clip = Transform(Matrix, {InWorld.X, InWorld.Y, InWorld.Z, 1});
	if (!std::isfinite(Clip.W) || !std::isfinite(Clip.X) || !std::isfinite(Clip.Y) || !std::isfinite(Clip.Z) ||
	    Clip.W <= 0 || Clip.Z < 0 || Clip.Z > Clip.W || std::abs(Clip.X) > Clip.W || std::abs(Clip.Y) > Clip.W)
	{
		return {};
	}
	return FVec3{InBounds.X + (Clip.X / Clip.W + 1) * .5f * Width, InBounds.Y + (1 - Clip.Y / Clip.W) * .5f * Height,
	             Clip.W};
}
} // namespace Hyperion
