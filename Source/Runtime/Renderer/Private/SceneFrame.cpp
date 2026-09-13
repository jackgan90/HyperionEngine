#include "SessionMaterialsInternal.h"
#include <cmath>

namespace Hyperion
{
namespace
{
constexpr auto SceneIndex = static_cast<std::size_t>(EMaterialScope::Scene);

struct FSceneLighting
{
	FVec3 Direction{0, 0, 1};
	FVec3 Direct;
	FVec3 Ambient;
	bool bShadows{};
};

FSceneLighting Lighting(const FSceneMetadata& InMetadata)
{
	FSceneLighting Result;
	if (InMetadata.Settings.MainDirectionalLight)
	{
		const auto Entry = InMetadata.DirectionalLights.find(*InMetadata.Settings.MainDirectionalLight);
		if (Entry != InMetadata.DirectionalLights.end() && Entry->second.bEnabled)
		{
			const auto& Light = Entry->second;
			Result.Direction = Light.SurfaceToLight;
			Result.Direct = ScaleVector(Light.Light.Color, Light.Light.Intensity);
			Result.bShadows =
			    Light.Light.bCastShadows && (Result.Direct.X > 0 || Result.Direct.Y > 0 || Result.Direct.Z > 0);
		}
	}
	if (InMetadata.Settings.EnvironmentLight)
	{
		const auto Entry = InMetadata.EnvironmentLights.find(*InMetadata.Settings.EnvironmentLight);
		if (Entry != InMetadata.EnvironmentLights.end() && Entry->second.bEnabled)
		{
			Result.Ambient = ScaleVector(Entry->second.Light.Color, Entry->second.Light.Intensity);
		}
	}
	return Result;
}

const FPublishedSceneCamera* Camera(const FSceneMetadata& InMetadata, std::optional<FSceneHandle> InHandle)
{
	if (!InHandle)
	{
		return nullptr;
	}
	const auto Entry = InMetadata.Cameras.find(*InHandle);
	return Entry != InMetadata.Cameras.end() && Entry->second.bEnabled ? &Entry->second : nullptr;
}

FMat4 CameraView(const FSceneCameraPose& InPose)
{
	const auto& Right = InPose.Right;
	const auto& Up = InPose.Up;
	const auto& Forward = InPose.Forward;
	// Pose already owns an orthonormal basis. Eye + unit Forward loses direction at large coordinates.
	return {{Right.X, Up.X, -Forward.X, 0, Right.Y, Up.Y, -Forward.Y, 0, Right.Z, Up.Z, -Forward.Z, 0,
	         -Dot(Right, InPose.Eye), -Dot(Up, InPose.Eye), Dot(Forward, InPose.Eye), 1}};
}

FResolvedSceneFrame ResolveView(const FSceneMetadata& InMetadata, const FSceneViewRequest& InRequest)
{
	if (InRequest.Camera && InRequest.Camera->Scene != InMetadata.Token.LogicalSceneIdentity)
	{
		throw std::invalid_argument("View request camera belongs to another scene");
	}
	FResolvedSceneFrame Result;
	auto& View = Result.View;
	View.Width = InRequest.Width;
	View.Height = InRequest.Height;
	View.Identity = InRequest.Identity;
	View.Usage = InRequest.Usage;
	View.Viewport = InRequest.Viewport;
	View.DepthConvention = InRequest.DepthConvention;
	View.CullingMode = InRequest.CullingMode;
	View.CullingViewProjection = InRequest.CullingViewProjection;
	View.bInstanceBatching = InRequest.bInstanceBatching;
	View.Parameters = InRequest.Parameters;
	View.PassParameters = InRequest.PassParameters;
	const auto Viewport = InRequest.Viewport.value_or(FViewport{0, 0, float(View.Width), float(View.Height)});
	if (!std::isfinite(Viewport.Width) || !std::isfinite(Viewport.Height) || Viewport.Width < 0 ||
	    Viewport.Height < 0 || !std::isfinite(Viewport.X) || !std::isfinite(Viewport.Y) ||
	    !std::isfinite(Viewport.MinDepth) || !std::isfinite(Viewport.MaxDepth) || Viewport.MinDepth < 0 ||
	    Viewport.MaxDepth > 1 || Viewport.MinDepth > Viewport.MaxDepth || InRequest.Identity == 0 ||
	    (InRequest.DepthConvention != EDepthConvention::Standard &&
	     InRequest.DepthConvention != EDepthConvention::Reversed) ||
	    (InRequest.CullingMode != ESceneCullingMode::None && InRequest.CullingMode != ESceneCullingMode::Linear &&
	     InRequest.CullingMode != ESceneCullingMode::Bvh))
	{
		throw std::invalid_argument("Invalid scene view viewport or identity");
	}
	if (!View.Width || !View.Height || Viewport.Width == 0 || Viewport.Height == 0)
	{
		Result.CameraStatus = ESceneCameraStatus::EmptyViewport;
		return Result;
	}
	Result.Camera = InRequest.Camera ? InRequest.Camera : InMetadata.Settings.DefaultCamera;
	const auto* Selected = Camera(InMetadata, Result.Camera);
	bool bFallback{};
	if (!Selected && InRequest.Camera)
	{
		Result.Camera = InMetadata.Settings.DefaultCamera;
		Selected = Camera(InMetadata, Result.Camera);
		bFallback = true;
	}
	if (!Selected)
	{
		Result.Camera.reset();
		return Result;
	}
	const auto& Lens = Selected->Camera;
	const auto& Pose = Selected->Pose;
	View.Eye = Pose.Eye;
	View.Camera = FRenderCamera{Pose.Forward, Pose.Up, Lens.VerticalRadians, Lens.Near, Lens.Far};
	View.SceneCamera = Result.Camera;
	View.ViewProjection = Multiply(
	    Perspective(Lens.VerticalRadians, Viewport.Width / Viewport.Height, Lens.Near, Lens.Far, View.DepthConvention),
	    CameraView(Pose));
	Result.CameraStatus = bFallback ? ESceneCameraStatus::DefaultFallback : ESceneCameraStatus::Active;
	return Result;
}
} // namespace

FResolvedSceneFrame FRenderSession::ResolveSceneFrame(const FSceneFrameSeed& InSeed, const FSceneViewRequest& InRequest)
{
	Tasks.Require({EDomain::Render});
	if (bClosed || !InSeed.Frame || InSeed.Frame->Session != MaterialState->Identity ||
	    InSeed.Frame->Frame < MaterialState->LastFrame)
	{
		throw std::invalid_argument("Foreign, stale or empty scene frame seed");
	}
	const auto Metadata = Scene.ResolveMetadata(InSeed.Token);
	MaterialState->Providers.ValidateSceneInputs(InRequest.Parameters);
	MaterialState->Providers.ValidateSceneInputs(InRequest.PassParameters);
	auto Result = ResolveView(*Metadata, InRequest);
	const auto& Cached = MaterialState->ResolvedSceneFrame;
	if (Cached && Cached->Frame == InSeed.Frame->Frame && Cached->SceneToken == InSeed.Token)
	{
		Result.Frame = Cached;
		return Result;
	}
	const auto Light = Lighting(*Metadata);
	auto Values = MaterialState->Providers.WithoutSceneInputs(InSeed.Frame->Inputs.Values[SceneIndex].Get());
	Values.push_back({"Engine.Scene.MainDirectionalLightDirection", FMaterialValue::Float(Light.Direction)});
	Values.push_back({"Engine.Scene.MainDirectionalLightColor", FMaterialValue::Float(Light.Direct)});
	Values.push_back({"Engine.Scene.AmbientColor", FMaterialValue::Float(Light.Ambient)});
	FMaterialInputValues Published(std::move(Values));
	auto& Effective = MaterialState->EffectiveSceneInputs;
	auto& Scope = Effective.Scopes[SceneIndex];
	const std::vector<std::uint64_t> Qualifiers{InSeed.Token.LogicalSceneIdentity, InSeed.Token.AttachmentEpoch};
	if (!Scope.Lifetime || Scope.Key.GetQualifiers() != Qualifiers || Effective.Values[SceneIndex] != Published)
	{
		auto Lifetime = Resources.CreateScopeLifetime();
		Scope.Key.Identity = MaterialState->Identity;
		Scope.Key.SetQualifiers(Qualifiers);
		++Scope.Key.Revision;
		Scope.Lifetime = std::move(Lifetime);
		Effective.Values[SceneIndex] = std::move(Published);
	}
	auto Frame = std::make_shared<FMaterialFrameContext>(*InSeed.Frame);
	Frame->SceneMetadata = Metadata;
	Frame->SceneToken = InSeed.Token;
	Frame->SceneResolutionOwner = Frame;
	Frame->bSceneShadows = Light.bShadows;
	Frame->Inputs.Values[SceneIndex] = Effective.Values[SceneIndex];
	Frame->Inputs.Scopes[SceneIndex] = Scope;
	MaterialState->ResolvedSceneFrame = Frame;
	Result.Frame = std::move(Frame);
	return Result;
}
} // namespace Hyperion
