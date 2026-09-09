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
	ShadowLight = Options.ShadowLight.value_or(Normalize(FVec3{-.45f, .8f, .65f}));
	LightAzimuth = std::atan2(ShadowLight.X, ShadowLight.Z) * 180 / 3.14159265f;
	LightElevation = std::asin(ShadowLight.Y) * 180 / 3.14159265f;
}

void FViewerApplication::UpdateShadowLight(int InFrame)
{
	if (Options.bBenchmarkLight && InFrame >= Options.BenchmarkWarmup)
	{
		const float Phase = float(InFrame - Options.BenchmarkWarmup) * .004f;
		ShadowLight = Normalize(FVec3{std::sin(Phase), .8f + .15f * std::sin(Phase * .7f), std::cos(Phase)});
	}
	RenderSession->SetSceneParameters(
	    {{"Engine.Scene.MainDirectionalLightDirection", FMaterialValue::Float(ShadowLight)},
	     {"Engine.Scene.MainDirectionalLightColor", FMaterialValue::Float(FVec3{3.f, 2.85f, 2.7f})},
	     {"Engine.Scene.AmbientColor", FMaterialValue::Float(FVec3{.22f, .25f, .3f})}});
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
		bool bLightChanged = Gui->Slider("Light azimuth", LightAzimuth, -180, 180);
		bLightChanged |= Gui->Slider("Light elevation", LightElevation, -90, 90);
		if (bLightChanged)
		{
			const float Azimuth = LightAzimuth * 3.14159265f / 180;
			const float Elevation = LightElevation * 3.14159265f / 180;
			ShadowLight = {std::sin(Azimuth) * std::cos(Elevation), std::sin(Elevation),
			               std::cos(Azimuth) * std::cos(Elevation)};
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
