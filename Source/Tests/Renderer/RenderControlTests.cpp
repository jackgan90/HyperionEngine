#include "Hyperion/Reflection/Json.h"
#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/Renderer/DebugGeometry.h"
#include "Hyperion/Renderer/RenderBenchmark.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/Renderer/SceneViewport.h"
#include "Hyperion/Renderer/ShadowControls.h"
#include "Hyperion/Scene/Scene.h"
#include "Support/TestSupport.h"
#include <fstream>
#include <iostream>
#include <limits>

using namespace Hyperion;

void RunViewDiagnosticsTests(const std::filesystem::path& InCaptureDirectory);
void RunViewportChoiceTests();
void RunFrameTimingTests();

namespace
{
template<class F> void Rejects(F InAction)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void Settings()
{
	FRenderSettings Values;
	Values.Pipeline = ESceneRenderPipeline::Forward;
	Values.GBuffer = EGBufferPreset::HighPrecision;
	Values.bReversedZ = false;
	Values.Shadows.Distance = 80;
	Values.Contact.bEnabled = true;
	Values.Exposure = .05f;
	FSceneViewportOptions ViewOptions;
	ViewOptions.Exposure = Values.Exposure;
	ValidateViewportOptions(ViewOptions, ViewOptions);
	const auto Path = std::filesystem::absolute("render-control-settings.json");
	WriteRenderSettings(Path, Values);
	const auto Loaded = LoadRenderSettings(Path);
	HYP_CHECK(WriteJson(WriteRecord(RecordType<FRenderSettings>(), &Values)) ==
	          WriteJson(WriteRecord(RecordType<FRenderSettings>(), &Loaded)));
	HYP_CHECK(MakePipelineSettings(Loaded).Pipeline == ESceneRenderPipeline::Forward);
	Values.Exposure = std::numeric_limits<float>::quiet_NaN();
	Rejects(
	    [&]
	    {
		    WriteRenderSettings(Path, Values);
	    });
	HYP_CHECK(LoadRenderSettings(Path).Exposure == Loaded.Exposure);
	Values = Loaded;
	Values.Contact.Steps = 0;
	Rejects(
	    [&]
	    {
		    ValidateRenderSettings(Values);
	    });
	Values = Loaded;
	Values.Pipeline = ESceneRenderPipeline::Count;
	Rejects(
	    [&]
	    {
		    ValidateRenderSettings(Values);
	    });
}

void SettingsCompatibility()
{
	const auto& Type = RecordType<FRenderSettings>();
	const std::array<std::string_view, 9> Keys{"pipeline", "gbuffer", "exposure", "debugMode", "clusteredLighting",
	                                           "contact",  "shadows", "vsync",    "reversedZ"};
	HYP_CHECK(Type.Id == "hyperion.render.settings" && Type.Version == 1 && Type.Members.size() == Keys.size());
	for (std::size_t Index = 0; Index < Keys.size(); ++Index)
	{
		HYP_CHECK(Type.Members[Index].Id == Keys[Index] && !Type.Members[Index].Options.bRequired);
	}
	const FRenderSettings Defaults;
	HYP_CHECK(Defaults.Pipeline == ESceneRenderPipeline::Deferred && Defaults.GBuffer == EGBufferPreset::Compact &&
	          Defaults.DebugMode == EGBufferVisualizer::Lit);
	HYP_CHECK(Defaults.Exposure == 1 && Defaults.bVsync && Defaults.bReversedZ);
	HYP_CHECK(Type.Members[0].Shape().Kind == ERecordValueKind::String);
	HYP_CHECK(Type.Members[1].Shape().Kind == ERecordValueKind::String);
	HYP_CHECK(Type.Members[3].Shape().Kind == ERecordValueKind::UnsignedInteger);
	HYP_CHECK(Type.Members[3].Shape().ElementBytes == 4);
	FArchiveNode::FObject Examples;
	for (const std::string Pipeline : {"deferred", "forward"})
	{
		for (const std::string Preset : {"compact", "high"})
		{
			FRenderSettings Values;
			Values.Pipeline = ParseSceneRenderPipeline(Pipeline);
			Values.GBuffer = ParseGBufferPreset(Preset);
			Values.DebugMode = EGBufferVisualizer::GeometryNormal;
			Values.Exposure = .05f;
			const auto Wire = WriteRecordWire(Type, &Values);
			const auto Loaded = ReadRecordWire(Type, Wire);
			HYP_CHECK(WriteJson(WriteRecordWire(Type, Loaded.get())) == WriteJson(Wire));
			Examples.emplace(Pipeline + "/" + Preset, FArchiveNode(FArchiveNode::FObject{
			                                              {"record", WriteRecord(Type, &Values)}, {"wire", Wire}}));
		}
	}
	const FArchiveNode Snapshot(FArchiveNode::FObject{{"schema", RecordWireSchema(Type)},
	                                                  {"defaults", WriteRecordWire(Type, &Defaults)},
	                                                  {"examples", FArchiveNode(std::move(Examples))}});
	std::ofstream File("render-option-render-contract.json", std::ios::binary);
	File << WriteJson(Snapshot);
	HYP_CHECK(File.good());
}

void StrictOptionConversions()
{
	for (const auto* Pipeline : {"deferred", "forward"})
	{
		for (const auto* Preset : {"compact", "high"})
		{
			FRenderSettings Values;
			Values.Pipeline = ParseSceneRenderPipeline(Pipeline);
			Values.GBuffer = ParseGBufferPreset(Preset);
			Values.DebugMode = EGBufferVisualizer::GeometryNormal;
			Values.Exposure = 1.75f;
			const auto Before = WriteRecordWire(RecordType<FRenderSettings>(), &Values);
			Values.bVsync = !Values.bVsync;
			auto Expected = Before;
			std::get<FArchiveNode::FObject>(Expected.Value)["vsync"] = WriteValue(Values.bVsync);
			HYP_CHECK(WriteJson(WriteRecordWire(RecordType<FRenderSettings>(), &Values)) == WriteJson(Expected));
			const auto Converted = MakePipelineSettings(Values);
			HYP_CHECK(Converted.Pipeline == ParseSceneRenderPipeline(Pipeline));
			HYP_CHECK(Converted.GBuffer ==
			          (std::string_view(Preset) == "high" ? FGBufferLayout::HighPrecision() : FGBufferLayout{}));
			HYP_CHECK(Converted.DebugMode == EGBufferVisualizer::GeometryNormal &&
			          Converted.Exposure == Values.Exposure);
		}
	}
	FRenderSettings Invalid;
	Invalid.Pipeline = ESceneRenderPipeline::Count;
	Rejects(
	    [&]
	    {
		    (void)MakePipelineSettings(Invalid);
	    });
	Invalid = {};
	Invalid.GBuffer = EGBufferPreset::Count;
	Rejects(
	    [&]
	    {
		    (void)MakePipelineSettings(Invalid);
	    });
	Invalid = {};
	for (const auto Mode : {7u, std::numeric_limits<std::uint32_t>::max()})
	{
		Invalid.DebugMode = static_cast<EGBufferVisualizer>(Mode);
		Rejects(
		    [&]
		    {
			    (void)MakePipelineSettings(Invalid);
		    });
		Rejects(
		    [&]
		    {
			    ValidateRenderSettings(Invalid);
		    });
		FSceneViewportOptions View;
		View.Visualizer = Mode;
		Rejects(
		    [&]
		    {
			    ValidateViewportOptions(View, View);
		    });
	}
}

void LightShadowProperties()
{
	FSceneDirectionalLight Light;
	auto Legacy = WriteRecord(RecordType<FSceneDirectionalLight>(), &Light);
	auto& Envelope = std::get<FArchiveNode::FObject>(Legacy.Value);
	Envelope["version"] = WriteValue(std::uint32_t(1));
	std::get<FArchiveNode::FObject>(Envelope.at("fields").Value).erase("shadowSettings");
	const auto Migrated =
	    std::static_pointer_cast<FSceneDirectionalLight>(ReadRecord(RecordType<FSceneDirectionalLight>(), Legacy));
	HYP_CHECK(!Migrated->ShadowSettings && Migrated->bCastShadows);
	Light.ShadowSettings.emplace();
	Light.ShadowSettings->Directional.Resolution = 1024;
	Light.ShadowSettings->Contact.bEnabled = true;
	const auto Loaded = std::static_pointer_cast<FSceneDirectionalLight>(
	    ReadRecord(RecordType<FSceneDirectionalLight>(),
	               ParseJson(WriteJson(WriteRecord(RecordType<FSceneDirectionalLight>(), &Light)))));
	HYP_CHECK(*Loaded == Light);
	const FSceneHandle Handle{1, 0, 1};
	FSceneMetadata Metadata;
	Metadata.Lighting.Directional.Handle = Handle;
	Metadata.DirectionalLights[Handle] = {Light, {0, 1, 0}, true};
	FCascadedShadowSettings Shadows;
	Shadows.PreviewViewport = {3, 4, 128, 128};
	FContactShadowSettings Contact;
	ResolveSceneLightShadows(&Metadata, Shadows, Contact);
	HYP_CHECK(Shadows.Resolution == 1024 && Contact.bEnabled && Shadows.PreviewViewport->X == 3);
	Metadata.DirectionalLights[Handle].Light.ShadowSettings.reset();
	Shadows = {};
	Contact = {};
	ResolveSceneLightShadows(&Metadata, Shadows, Contact);
	HYP_CHECK(Shadows.Resolution == 2048 && !Contact.bEnabled);
	Light.ShadowSettings->Directional.Resolution = 1;
	Rejects(
	    [&]
	    {
		    ValidateSceneDirectionalLight(Light);
	    });
	Light.ShadowSettings->Directional.Resolution = 2048;
	Light.ShadowSettings->Contact.Steps = 0;
	Rejects(
	    [&]
	    {
		    ValidateSceneDirectionalLight(Light);
	    });
}

void HudOptionsAndExposure()
{
	FSceneViewportOptions Supported;
	Supported.StatusHud = false;
	Supported.ProfilingHud = false;
	Supported.ProfilingCategories = 1;
	Supported.Visualizer = 0;
	auto Patch = Supported;
	Patch.ProfilingCategories = 255;
	Patch.Visualizer = 6;
	ValidateViewportOptions(Patch, Supported);
	Patch.Visualizer = 7;
	Rejects(
	    [&]
	    {
		    ValidateViewportOptions(Patch, Supported);
	    });
	Patch.Visualizer = 0;
	Patch.ProfilingCategories = 256;
	Rejects(
	    [&]
	    {
		    ValidateViewportOptions(Patch, Supported);
	    });
	Rejects(
	    [&]
	    {
		    ValidateViewportOptions(Supported, {});
	    });
	FRenderSettings Settings;
	FRecordDraft Draft(RecordType<FRenderSettings>(), &Settings);
	Draft.GetValues()["exposure"] = WriteValue(.05);
	Draft.ApplyToCandidate(&Settings);
	HYP_CHECK(Settings.Exposure == .05f);
	FRecordDraft Rounded(RecordType<FRenderSettings>(), &Settings);
	Rounded.ApplyToCandidate(&Settings);
	HYP_CHECK(Settings.Exposure == .05f);
}

void ClipLines()
{
	const FVec4 Viewport{10, 20, 110, 120};
	const auto Clipped = ProjectDebugLine({{-2, 0, .5f}, {2, 0, .5f}}, Identity(), Viewport);
	HYP_CHECK(Clipped && (*Clipped)[0].X == 10 && (*Clipped)[1].X == 110);
	HYP_CHECK(!ProjectDebugLine({{0, 0, -1}, {0, 1, -2}}, Identity(), Viewport));
	HYP_CHECK(ProjectDebugLine({{0, 0, -1}, {0, 0, .5f}}, Identity(), Viewport));
	FSceneNode Light;
	Light.PointLight() = FScenePointLight{};
	const std::array Views{FSceneNodeView{{}, &Light, Identity(), true}};
	HYP_CHECK(BuildSceneDebugLines(Views, false, true).size() == 192);
	HYP_CHECK(BuildSceneDebugLines(Views, true, false).empty());
	FScene Scene;
	Light.Id = "degenerate-point";
	Light.Local() = Multiply(Translation({3, 4, 5}), Scale({1, 0, 0}));
	const auto Handle = Scene.AddNode(Light);
	FSceneNodeView View;
	HYP_CHECK(Scene.GetNodeView(Handle, View));
	const auto Lines = BuildSceneDebugLines(std::span(&View, 1), false, true);
	HYP_CHECK(Lines.size() == 192);
	HYP_CHECK(Lines.front().A.X == 3 + Light.PointLight()->Range && Lines.front().A.Y == 4 && Lines.front().A.Z == 5);
}

void TimingIdentity()
{
	std::array<FRenderBenchmarkSample, 2> Samples;
	Samples[0].Device.SubmittedFrames = 7;
	Samples[1].Device.SubmittedFrames = 9;
	FGpuTimingCapture Capture;
	Capture.Frames.resize(2);
	Capture.Frames[0].Frame = 9;
	Capture.Frames[1].Frame = 7;
	MatchBenchmarkTimings(Samples, Capture);
	HYP_CHECK(Samples[0].Device.GpuTiming.Frame == 7 && Samples[1].Device.GpuTiming.Frame == 9);
	Capture.Frames[0].Frame = 7;
	Rejects(
	    [&]
	    {
		    MatchBenchmarkTimings(Samples, Capture);
	    });
	Capture.Frames[0].Frame = 10;
	Rejects(
	    [&]
	    {
		    MatchBenchmarkTimings(Samples, Capture);
	    });
	Capture.Frames[0].Frame = 9;
	Capture.DroppedFrames = 1;
	Rejects(
	    [&]
	    {
		    MatchBenchmarkTimings(Samples, Capture);
	    });
}
} // namespace

int main(int InCount, char** InArguments)
{
	try
	{
		if (InCount == 3 && std::string_view(InArguments[1]) == "--capture-view-diagnostics")
		{
			RunViewDiagnosticsTests(std::filesystem::path(InArguments[2]));
			return 0;
		}
		HYP_CHECK(InCount == 1);
		Settings();
		SettingsCompatibility();
		StrictOptionConversions();
		LightShadowProperties();
		HudOptionsAndExposure();
		RunViewportChoiceTests();
		ClipLines();
		TimingIdentity();
		RunFrameTimingTests();
		RunViewDiagnosticsTests({});
		std::cout << "Render settings, clipped diagnostics and timing identity passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
