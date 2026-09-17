#include "ViewerApplication.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
bool ParseShadowOption(FOptions& InOptions, const std::string& InArg, int InArgc, char** InArgv, int& InIndex)
{
	if (InArg == "--no-shadows")
	{
		InOptions.Shadows.bEnabled = false;
	}
	else if (InArg == "--shadow-resolution" && InIndex + 1 < InArgc)
	{
		const int Resolution = std::stoi(InArgv[++InIndex]);
		if (Resolution != 1024 && Resolution != 2048)
		{
			throw std::invalid_argument("Shadow resolution must be 1024 or 2048");
		}
		InOptions.Shadows.Resolution = static_cast<unsigned>(Resolution);
	}
	else if (InArg == "--shadow-distance" && InIndex + 1 < InArgc)
	{
		InOptions.Shadows.Distance = std::stof(InArgv[++InIndex]);
		if (!std::isfinite(InOptions.Shadows.Distance) || InOptions.Shadows.Distance <= 0)
		{
			throw std::invalid_argument("Shadow distance must be finite and positive");
		}
	}
	else if (InArg == "--shadow-debug" && InIndex + 1 < InArgc)
	{
		const int Mode = std::stoi(InArgv[++InIndex]);
		if (Mode < 0 || Mode > 5)
		{
			throw std::invalid_argument("Shadow debug accepts 0..5");
		}
		InOptions.Shadows.DebugMode = static_cast<unsigned>(Mode);
	}
	else if (InArg == "--shadow-light" && InIndex + 3 < InArgc)
	{
		FVec3 Light;
		Light.X = std::stof(InArgv[++InIndex]);
		Light.Y = std::stof(InArgv[++InIndex]);
		Light.Z = std::stof(InArgv[++InIndex]);
		if (!IsFinite(Light) || Length(Light) < .0001f || !std::isfinite(Length(Light)))
		{
			throw std::invalid_argument("Shadow light needs a finite nonzero surface-to-light vector");
		}
		InOptions.ShadowLight = Normalize(Light);
	}
	else if (InArg == "--benchmark-light")
	{
		InOptions.bBenchmarkLight = true;
	}
	else
	{
		return false;
	}
	return true;
}

void FViewerApplication::InitializeShadowSettings()
{
	ShadowSettings = Options.Shadows;
}

FSceneInstance* FViewerApplication::GetSceneInstance()
{
	return ScenePlugin ? &ScenePlugin->GetSceneInstance() : ModelPlugin ? &ModelPlugin->GetSceneInstance() : nullptr;
}

void FViewerApplication::SetSceneLightDirection(FVec3 InDirection)
{
	auto* Scene = GetSceneInstance();
	if (!Scene || !Scene->GetStatus().bLoaded)
	{
		return;
	}
	auto Selections = Scene->GetSettings();
	if (!Selections.MainDirectionalLight)
	{
		Selections.MainDirectionalLight = Scene->AddNode(MakeSceneDirectionalLightNode({}));
		Scene->SetSettings(Selections);
	}
	FSceneNodeView View;
	Scene->GetNodeView(*Selections.MainDirectionalLight, View);
	const auto Pose = ExtractScenePose(View.World);
	const auto Forward = ScaleVector(Normalize(InDirection), -1);
	const FVec3 Up = std::abs(Forward.Y) > .999f ? FVec3{0, 0, 1} : FVec3{0, 1, 0};
	Scene->SetWorldTransform(*Selections.MainDirectionalLight,
	                         SceneCameraTransform(Pose.Eye, Add(Pose.Eye, Forward), Up));
}

void FViewerApplication::UpdateShadowLight(int InFrame)
{
	const auto* Scene = GetSceneInstance();
	if (!Scene || !Scene->GetStatus().bLoaded)
	{
		return;
	}
	if (Options.ShadowLight && !bShadowLightApplied)
	{
		SetSceneLightDirection(*Options.ShadowLight);
		bShadowLightApplied = true;
	}
	if (Options.bBenchmarkLight && InFrame >= Options.BenchmarkWarmup)
	{
		const float Phase = float(InFrame - Options.BenchmarkWarmup) * .004f;
		SetSceneLightDirection(Normalize(FVec3{std::sin(Phase), .8f + .15f * std::sin(Phase * .7f), std::cos(Phase)}));
	}
}

void FViewerApplication::DrawShadowGui()
{
	const auto Size = Gui->DisplaySize();
	const bool bPreview = ShadowSettings.bEnabled && ShadowSettings.DebugMode >= 2;
	const float PreviewSize = std::min({256.f, Size.X, Size.Y / 3});
	const float PanelHeight = std::min(680.f, Size.Y - (bPreview ? PreviewSize + 48 : 32));
	if (bPreview)
	{
		ShadowSettings.PreviewViewport = {std::min(368.f, std::max(0.f, Size.X - PreviewSize)),
		                                  std::max(0.f, Size.Y - PreviewSize - 16), PreviewSize, PreviewSize};
	}
	if (Gui->BeginPanel("Directional shadows", {368, 16}, {400, std::max(100.f, PanelHeight)}))
	{
		Gui->Checkbox("Enable cascaded shadows", ShadowSettings.bEnabled);
		Gui->Text("4 cascades | every frame | " + std::to_string(ShadowSettings.Resolution) + " px");
		if (Gui->Button("Switch 1024 / 2048"))
		{
			ShadowSettings.Resolution = ShadowSettings.Resolution == 2048 ? 1024 : 2048;
		}
		Gui->Slider("Shadow distance", ShadowSettings.Distance, 5, 200);
		Gui->Slider("Split lambda", ShadowSettings.SplitLambda, 0, 1);
		Gui->Slider("Normal (texels)", ShadowSettings.NormalOffset, 0, 2);
		Gui->Slider("Depth (texels)", ShadowSettings.ReceiverBias, 0, 2);
		Gui->Slider("Cascade blend", ShadowSettings.BlendFraction, .01f, .25f);
		Gui->Slider("Distance fade", ShadowSettings.FadeFraction, .01f, .5f);
		auto* Scene = GetSceneInstance();
		const auto Handle = Scene ? Scene->GetSettings().MainDirectionalLight : std::optional<FSceneHandle>{};
		FSceneNodeView LightView;
		if (Handle && Scene->GetNodeView(*Handle, LightView))
		{
			auto Light = *LightView.Node->DirectionalLight();
			bool bLightChanged = Gui->InputVector("Main light color", Light.Color);
			bLightChanged |= Gui->InputFloat("Main light intensity", Light.Intensity);
			bLightChanged |= Gui->Checkbox("Main light casts shadows", Light.bCastShadows);
			if (bLightChanged)
			{
				try
				{
					Scene->SetDirectionalLight(*Handle, Light);
				}
				catch (const std::exception& Failure)
				{
					Gui->TextWrapped(Failure.what());
				}
			}
			const auto Direction = ScaleVector(ExtractScenePose(LightView.World).Forward, -1);
			float AzimuthDegrees = std::atan2(Direction.X, Direction.Z) * 180 / 3.14159265f;
			float ElevationDegrees = std::asin(std::clamp(Direction.Y, -1.f, 1.f)) * 180 / 3.14159265f;
			bool bChanged = Gui->Slider("Light azimuth", AzimuthDegrees, -180, 180);
			bChanged |= Gui->Slider("Light elevation", ElevationDegrees, -90, 90);
			if (bChanged)
			{
				const float Azimuth = AzimuthDegrees * 3.14159265f / 180;
				const float Elevation = ElevationDegrees * 3.14159265f / 180;
				try
				{
					SetSceneLightDirection({std::sin(Azimuth) * std::cos(Elevation), std::sin(Elevation),
					                        std::cos(Azimuth) * std::cos(Elevation)});
				}
				catch (const std::exception& Failure)
				{
					Gui->TextWrapped(Failure.what());
				}
			}
		}
		else
		{
			Gui->Text("No main directional light selected");
		}
		const std::array Names{"Shaded", "Cascade tint", "Depth 0", "Depth 1", "Depth 2", "Depth 3"};
		Gui->Text(std::string("Display: ") + Names[ShadowSettings.DebugMode]);
		if (Gui->Button("Cycle shadow display"))
		{
			ShadowSettings.DebugMode = (ShadowSettings.DebugMode + 1) % Names.size();
		}
		Gui->Separator();
		Gui->Text("Depth payload " + std::to_string(PipelineStatistics.ShadowTextureBytes / (1024 * 1024)) + " MiB");
		Gui->Text("Pipeline CPU " + std::to_string(PipelineStatistics.PreparationMilliseconds) + " ms");
		for (const auto& View : PipelineStatistics.Views)
		{
			Gui->Text(View.Usage + ": " + std::to_string(View.Visibility.VisibleItems) + " items / " +
			          std::to_string(View.Visibility.Draws) + " draws");
		}
		Gui->Text("GPU sample frame " + std::to_string(Metrics.Device.GpuTiming.Frame));
		for (const auto& Pass : Metrics.Device.GpuTiming.Passes)
		{
			Gui->Text(Pass.Name + ": " + std::to_string(Pass.Milliseconds) + " ms");
		}
	}
	Gui->EndPanel();
}
} // namespace Hyperion
