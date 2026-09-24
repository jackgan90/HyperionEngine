#include "Hyperion/Core/Core.h"
#include "Hyperion/Renderer/SceneEditTarget.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include "ViewerApplication.h"

namespace Hyperion
{
namespace
{
void VerifySceneDraws(ISceneEditor& InPlugin, std::size_t InDraws)
{
	auto& Scene = InPlugin.GetSceneInstance();
	for (const auto& Model : Scene.GetModels())
	{
		for (const auto& Draw : Scene.GetDrawResults(Model.Handle))
		{
			if (!Draw.Error.empty())
			{
				throw std::runtime_error("Scene draw failed (" + Draw.Usage + "): " + Draw.Error);
			}
		}
	}
	if (!InPlugin.Ready() || InDraws == 0)
	{
		throw std::runtime_error("Scene model verification requires ready models and submitted draws");
	}
}
} // namespace

void FViewerPlugin::SaveSettingsAsync(const std::filesystem::path& InPath)
{
	(void)SaveApplicationSettings(InPath);
}

FApplicationSettingsState FViewerPlugin::ApplicationSettings() const
{
	return {Settings, SettingsRevision, bActiveReversedZ};
}

FRenderHealth FViewerPlugin::RenderHealth()
{
	FRenderHealth Result;
	Result.Frame = Metrics.ResultFrame;
	Result.bReady = SceneProducer && SceneProducer->Ready();
	if (auto* Scene = GetSceneInstance())
	{
		Result.Error = ScenePreparationError(*Scene);
	}
	if (SceneProducer && !SceneProducer->Error().empty())
	{
		Result.Error = SceneProducer->Error();
	}
	return Result;
}

FRenderDiagnostics FViewerPlugin::RenderDiagnostics()
{
	const auto Health = RenderHealth();
	FRenderDiagnostics Result;
	Result.Frame = Health.Frame;
	Result.bReady = Health.bReady;
	Result.SceneError = Health.Error;
	Result.Pipeline = PipelineStatistics;
	SetDeviceDiagnostics(Result, Metrics.Device);
	return Result;
}

FSceneComponentDiagnostics FViewerPlugin::ComponentDiagnostics(FSceneHandle InHandle, std::string_view InComponent)
{
	auto* Scene = GetSceneInstance();
	if (!Scene || !Scene->FindNode(InHandle))
	{
		throw FSceneEditError("stale_handle", "Object is no longer in this scene");
	}
	const auto Value = Scene->GetComponentDiagnostics(InHandle, InComponent);
	if (!Value)
	{
		throw FSceneEditError("not_found", "Component has no published rendering diagnostics");
	}
	return *Value;
}

void FViewerPlugin::EditApplicationSettings(std::uint64_t InRevision, const FAppSettings& InSettings)
{
	if (bFinished || bStopped)
	{
		throw FSceneEditError("unavailable", "Application is stopping");
	}
	if (InRevision != SettingsRevision)
	{
		throw FSceneEditError("stale_revision", "Application settings changed");
	}
	if (!EqualAppSettings(Settings, InSettings))
	{
		ApplyAppSettings(Settings, InSettings);
		++SettingsRevision;
	}
}

TAsyncResult<bool> FViewerPlugin::SaveApplicationSettings(const std::filesystem::path& InPath)
{
	const auto Text = EncodeReflected(SettingsType(), &Settings);
	const auto Bytes = std::as_bytes(std::span(Text));
	Services->FileWrites.reserve(Services->FileWrites.size() + 1);
	auto Write = Services->IO.WriteAsync(InPath.empty() ? Options.Config : InPath, {Bytes.begin(), Bytes.end()});
	Services->FileWrites.push_back(Write.Task());
	return Write;
}

void FViewerPlugin::SaveScreenshot(FImage InImage, const FAppSettings& InSettings, const std::string& InSceneError)
{
	if (Options.bVerifyModel && !InSceneError.empty())
	{
		throw std::runtime_error(InSceneError);
	}
	const auto Path = Options.Capture.empty() ? std::filesystem::path(HYP_SOURCE_DIR) / "out/captures" /
	                                                ("capture-" + std::to_string(ClockNanoseconds()) + ".png")
	                                          : Options.Capture;
	VerifyImage(InImage, InSettings, Options);
	Services->FileWrites.push_back(
	    DispatchAsync<bool>(Services->Tasks, {EDomain::Worker},
	                        [Services = Services.get(), Path, Snapshot = std::move(InImage)]
	                        {
		                        return *Services->IO.WriteAsync(Path, EncodePng(Snapshot)).Get(Services->Tasks);
	                        })
	        .Task());
	bCaptured = true;
	Log(ELogLevel::Info, "Screenshot save queued: " + Path.string());
}

void FViewerPlugin::VerifyOutputs()
{
	if (Options.bExerciseContactShadows && !bContactExerciseCompleted)
	{
		throw std::runtime_error("Contact GUI exercise did not verify off/on/off/on with a ready nonempty scene");
	}
	if (Options.bExerciseContactShadows && Options.bExercise && !bContactWindowCompleted)
	{
		throw std::runtime_error("Contact GUI exercise did not verify active resize/minimize/restore");
	}
	Log(ELogLevel::Info, std::string("Depth convention: active=") + (bActiveReversedZ ? "reversed" : "standard") +
	                         "; configured=" + (Settings.bReversedZ ? "reversed" : "standard"));
	const auto Progress = FramePipeline->Progress();
	Log(ELogLevel::Info, "CPU frames: submitted=" + std::to_string(Progress.Submitted) +
	                         "; render=" + std::to_string(Progress.RenderCompleted) +
	                         "; rhi=" + std::to_string(Progress.RhiCompleted) +
	                         "; leads=" + std::to_string(Metrics.FrameLimits.MainLead) + "," +
	                         std::to_string(Metrics.FrameLimits.RenderLead));
	if (ScenePlugin)
	{
		if (Options.bVerifyModel)
		{
			VerifySceneDraws(*ScenePlugin, SceneStatistics.Draws);
		}
		Log(ELogLevel::Info, "Scene: " + ScenePlugin->Status() + " | groups=" + std::to_string(SceneStatistics.Groups) +
		                         " visits=" + std::to_string(SceneStatistics.VisitedNodes) +
		                         " group_tests=" + std::to_string(SceneStatistics.GroupTests) +
		                         " collects=" + std::to_string(SceneStatistics.CollectedPrimitives) +
		                         " items=" + std::to_string(SceneStatistics.VisibleItems) +
		                         " draws=" + std::to_string(SceneStatistics.Draws));
	}
	if (ModelPlugin)
	{
		Log(ELogLevel::Info, "Model: " + ModelPlugin->Status() + " | ready=" + std::to_string(ModelPlugin->Ready()));
	}
	const auto& Lights = PipelineStatistics.LocalLights;
	const auto& Depth = PipelineStatistics.HierarchicalDepth;
	Log(ELogLevel::Info,
	    "Contact shadows: active=" + std::to_string(PipelineStatistics.bContactShadows) +
	        " HZB consumers=" + std::to_string(Depth.Consumers) + " products=" + std::to_string(Depth.Products) +
	        " dispatches=" + std::to_string(Depth.Dispatches) + " bytes=" + std::to_string(Depth.Bytes));
	Log(ELogLevel::Info,
	    "Local lights: active=" + std::to_string(Lights.bActive) + " points=" + std::to_string(Lights.Points) +
	        " spots=" + std::to_string(Lights.Spots) + " visible=" +
	        std::to_string(Lights.VisiblePoints + Lights.VisibleSpots) + " draws=" + std::to_string(Lights.Draws) +
	        " clustered=" + std::to_string(Lights.bClustered) + " clusters=" + std::to_string(Lights.ClusterOccupied) +
	        "/" + std::to_string(Lights.ClusterCells) + " references=" + std::to_string(Lights.ClusterReferences));
	if (!Options.Capture.empty() && !bCaptured)
	{
		throw std::runtime_error("Requested capture was not produced");
	}
#if HYP_ENABLE_RENDERDOC
	if (!Options.RdcFrames.empty() &&
	    (!FrameCapture || FrameCapture->Status().CompletedCaptures != Options.RdcFrames.size()))
	{
		throw std::runtime_error("Requested RDC capture was not produced: " +
		                         (FrameCapture ? FrameCapture->Status().Message : std::string("plugin unavailable")));
	}
#else
	if (!Options.RdcFrames.empty())
	{
		throw std::runtime_error("Requested RDC capture was not produced: plugin unavailable");
	}
#endif
	if (!Options.SaveScene.empty())
	{
		if (!ScenePlugin || !ScenePlugin->Ready())
		{
			throw std::runtime_error("--save-scene requires a ready scene");
		}
		ScenePlugin->SaveAsync(Options.SaveScene).Get(Services->Tasks);
		Log(ELogLevel::Info, "Scene saved: " + Options.SaveScene.string());
	}
	if (!Options.SaveConfig.empty())
	{
		const auto Size = Window->LogicalSize();
		Settings.Width = static_cast<int>(Size.Width);
		Settings.Height = static_cast<int>(Size.Height);
		SaveSettingsAsync(Options.SaveConfig);
	}
}
} // namespace Hyperion
