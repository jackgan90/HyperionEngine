#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"

namespace Hyperion
{
void FEditorPlugin::CheckRenderControlsHud() const
{
	const auto Clip = ViewportRegion.Bounds;
	for (const auto* Name : {"hud/status", "hud/profiling"})
	{
		const auto Bounds = InspectionBounds.at(Name);
		if (Bounds.Z <= Bounds.X || Bounds.W <= Bounds.Y || Bounds.X < Clip.X || Bounds.Y < Clip.Y ||
		    Bounds.Z > Clip.Z || Bounds.W > Clip.W)
		{
			throw std::runtime_error("HUD missing or outside the viewport: " + std::string(Name) +
			                         " clip=" + std::to_string(Clip.X) + "," + std::to_string(Clip.Y) + "," +
			                         std::to_string(Clip.Z) + "," + std::to_string(Clip.W) +
			                         " bounds=" + std::to_string(Bounds.X) + "," + std::to_string(Bounds.Y) + "," +
			                         std::to_string(Bounds.Z) + "," + std::to_string(Bounds.W));
		}
	}
	if (InspectionBounds.at("hud/status").Z > InspectionBounds.at("hud/profiling").X)
	{
		throw std::runtime_error("Status and profiling HUDs overlap");
	}
	for (const auto* Name :
	     {"hud/status-toggle", "hud/profiling-toggle", "hud/visualizer", "hud/exposure", "hud/categories"})
	{
		const auto Bounds = InspectionBounds.at(Name);
		if (Bounds.Z <= Bounds.X || Bounds.X < Clip.X || Bounds.Z > Clip.Z || Bounds.W > Clip.Y)
		{
			throw std::runtime_error("Render toolbar control is clipped: " + std::string(Name));
		}
	}
	if (IsDirty())
	{
		throw std::runtime_error("HUD controls changed the scene document");
	}
}

void FEditorPlugin::ExerciseRenderControlsInput(std::vector<FInputEvent>& InEvents)
{
	if (ReadyFrames < 8)
	{
		return;
	}
	if (ExerciseStep >= 11)
	{
		ExerciseProfilingHudInput(InEvents);
		return;
	}
	switch (ExerciseStep)
	{
		case 0:
			ExerciseClick(InEvents, InspectionBounds["hud/status-toggle"]);
			return;
		case 1:
			ExerciseClick(InEvents, InspectionBounds["hud/profiling-toggle"]);
			return;
		case 2:
			CheckRenderControlsHud();
			PlacementCapture = Options.ExerciseRenderControls / "Hud.png";
			++ExerciseStep;
			return;
		case 3:
			ExerciseClick(InEvents, EditMenuBounds);
			return;
		case 4:
			ExerciseClick(InEvents, InspectionBounds["render/settings-menu"]);
			return;
		case 5:
			if (!bShowRenderSettings || InspectionBounds["render/pipeline"].Z <= InspectionBounds["render/pipeline"].X)
			{
				throw std::runtime_error("Render settings menu did not open its window");
			}
			PlacementCapture = Options.ExerciseRenderControls / "Settings.png";
			++ExerciseStep;
			return;
		case 6:
			bShowRenderSettings = false;
			SceneDocument.ReplaceSelection(FSceneSelection(Scene->GetLightingSelection().Directional.Handle));
			++ExerciseStep;
			return;
		case 7:
			if (!ExerciseLightPriorityInput(InEvents))
			{
				return;
			}
			ExerciseClick(InEvents, InspectionBounds[RecordType<FSceneTransform>().Id + "/header"]);
			return;
		case 8:
			ExerciseClick(InEvents, InspectionBounds["hyperion.scenedirectionallight/shadowSettings"]);
			return;
		case 9:
			if (!Scene->FindNode(*Selection)->DirectionalLight()->ShadowSettings || !IsDirty())
			{
				throw std::runtime_error("Light shadow Inspector did not author component values");
			}
			PlacementCapture = Options.ExerciseRenderControls / "Light.png";
			++ExerciseStep;
			return;
		case 10:
			Undo();
			if (Scene->FindNode(*Selection)->DirectionalLight()->ShadowSettings)
			{
				throw std::runtime_error("Shadow Inspector undo failed");
			}
			Redo();
			if (!Scene->FindNode(*Selection)->DirectionalLight()->ShadowSettings)
			{
				throw std::runtime_error("Shadow Inspector redo failed");
			}
			Undo();
			Window->Resize({1000, 900});
			Gui->SetApplicationScale(2);
			ExerciseWait = 0;
			++ExerciseStep;
			return;
	}
}

bool FEditorPlugin::ExerciseLightPriorityInput(std::vector<FInputEvent>& InEvents)
{
	if (LightPriorityExerciseStep >= 16)
	{
		return true;
	}
	const bool bSky = LightPriorityExerciseStep >= 8;
	const unsigned Phase = LightPriorityExerciseStep % 8;
	const auto Type = bSky ? RecordType<FSceneEnvironmentLight>().Id : RecordType<FSceneDirectionalLight>().Id;
	if (Phase == 0 && bSky)
	{
		auto Node = MakeSceneEnvironmentLightNode("priority-gui-sky");
		Node.EnvironmentLight()->Source = ESceneEnvironmentSource::ConstantColor;
		SceneDocument.ReplaceSelection(FSceneSelection(SceneDocument.CommitCreate(std::move(Node))));
	}
	if (Phase == 1)
	{
		const auto Step = ExerciseStep;
		ExerciseClick(InEvents, InspectionBounds.at(Type + "/priority"));
		if (Step == ExerciseStep)
		{
			return false;
		}
		ExerciseStep = Step;
	}
	else if (Phase >= 2 && Phase <= 5)
	{
		if (Phase == 4 && ExerciseWait++ == 0)
		{
			return false;
		}
		ExerciseWait = 0;
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = Phase < 4 ? EKey::A : EKey::Enter;
		Key.bDown = Phase == 2 || Phase == 4;
		Key.Modifiers = Phase == 2 ? 1 : 0;
		InEvents.push_back(Key);
		if (Phase == 3)
		{
			FInputEvent Text;
			Text.Type = EEventType::Text;
			Text.Text = "-7";
			InEvents.push_back(Text);
		}
	}
	else if (Phase == 6)
	{
		const auto* Node = Scene->FindNode(*Selection);
		const auto Priority = bSky ? Node->EnvironmentLight()->Priority : Node->DirectionalLight()->Priority;
		if (Priority != -7 || !IsDirty())
		{
			throw std::runtime_error("Priority Inspector did not commit a signed integer: " + Error);
		}
		PlacementCapture = Options.ExerciseRenderControls / (bSky ? "SkyPriority.png" : "DirectionalPriority.png");
	}
	else if (Phase == 7)
	{
		Undo();
		const auto* Node = Scene->FindNode(*Selection);
		if ((bSky ? Node->EnvironmentLight()->Priority : Node->DirectionalLight()->Priority) != 0)
		{
			throw std::runtime_error("Priority Inspector undo failed");
		}
		Redo();
		Node = Scene->FindNode(*Selection);
		if ((bSky ? Node->EnvironmentLight()->Priority : Node->DirectionalLight()->Priority) != -7)
		{
			throw std::runtime_error("Priority Inspector redo failed");
		}
		Undo();
		if (bSky)
		{
			Undo();
			SceneDocument.ReplaceSelection(FSceneSelection(Scene->GetLightingSelection().Directional.Handle));
		}
	}
	++LightPriorityExerciseStep;
	return false;
}

void FEditorPlugin::ExerciseProfilingHudInput(std::vector<FInputEvent>& InEvents)
{
	if (ExerciseStep >= 17)
	{
		ExerciseProfilingDetailsInput(InEvents);
		return;
	}
	const auto Profiling = GetProfilingStatus();
	switch (ExerciseStep)
	{
		case 11:
			if (++ExerciseWait < 8)
			{
				return;
			}
			CheckRenderControlsHud();
			PlacementCapture = Options.ExerciseRenderControls / "Narrow.png";
			ExerciseWait = 0;
			++ExerciseStep;
			return;
		case 12:
			if (!ExerciseWait)
			{
				// The narrow HUD was checked above; collection controls use unscrolled click bounds.
				Window->Resize({1600, 960});
				Gui->SetApplicationScale(1);
			}
			ExerciseClick(InEvents, InspectionBounds["hud/categories"]);
			return;
		case 13:
			ExerciseClick(InEvents, InspectionBounds["hud/category/1"]);
			return;
		case 14:
			if (ProfilingCategories != 3 || IsDirty() || Profiling.Mask)
			{
				throw std::runtime_error("Profiling category GUI changed collection or document state");
			}
			if (Profiling.bCompiled)
			{
				ChangeProfiling(ProfileCategoryMask(EProfileCategory::Frame), {});
			}
			++ExerciseStep;
			return;
		case 15:
			if (Profiling.bCompiled)
			{
				ExerciseClick(InEvents, InspectionBounds["hud/collect-gpu"]);
			}
			else
			{
				++ExerciseStep;
			}
			return;
	}
	if (Profiling.bCompiled)
	{
		const auto Expected = ProfileCategoryMask(EProfileCategory::Frame) | ProfileCategoryMask(EProfileCategory::Gpu);
		if (Profiling.Mask != Expected)
		{
			throw std::runtime_error("GPU collection GUI changed unrelated CPU category bits");
		}
		ChangeProfiling(0, {});
	}
	PlacementCapture = Options.ExerciseRenderControls / "Stats.png";
	++ExerciseStep;
}

void FEditorPlugin::ExerciseProfilingDetailsInput(std::vector<FInputEvent>& InEvents)
{
	switch (ExerciseStep)
	{
		case 17:
		{
			Gui->ClosePopups();
			Window->Resize({1600, 960});
			Gui->SetApplicationScale(1);
			FSceneViewportOptions ViewOptions;
			ViewOptions.ProfilingCategories = 0;
			ViewOptions.InstanceBatching = false;
			SetViewportOptions(ViewOptions);
			ExerciseWait = 0;
			++ExerciseStep;
			return;
		}
		case 18:
			if (++ExerciseWait >= 8)
			{
				++ExerciseStep;
				ExerciseWait = 0;
			}
			return;
		case 19:
		case 22:
			ExerciseClick(InEvents, InspectionBounds["hud/categories"]);
			return;
		case 20:
		case 23:
		{
			const auto Anchor = InspectionBounds.at("hud/categories");
			const auto Title = InspectionBounds.at("hud/menu-title");
			const float Padding = Gui->Scale(16);
			if (Title.X < Anchor.X || Title.X > Anchor.X + Padding || Title.Y < Anchor.W ||
			    Title.Y > Anchor.W + Padding)
			{
				throw std::runtime_error("Stats menu did not expand from the button's bottom-left corner");
			}
			ExerciseClick(InEvents, InspectionBounds["hud/category/6"]);
			return;
		}
		case 24:
			ExerciseClick(InEvents, InspectionBounds["hud/category/7"]);
			return;
		case 21:
			if (ProfilingCategories != 64 || IsDirty() || GetProfilingStatus().Mask)
			{
				throw std::runtime_error("Visibility category GUI did not update only the HUD state");
			}
			Gui->ClosePopups();
			HudUpdated = 0;
			PlacementCapture = Options.ExerciseRenderControls / "Visibility.png";
			++ExerciseStep;
			return;
	}
	if (ProfilingCategories != 128 || IsDirty() || GetProfilingStatus().Mask ||
	    !RenderStats.MainView().Batches.Fallbacks[static_cast<std::size_t>(ERenderBatchFallback::Disabled)])
	{
		throw std::runtime_error("Batching HUD requires the selected category and disabled-batching fallback data");
	}
	Gui->ClosePopups();
	HudUpdated = 0;
	PlacementCapture = Options.ExerciseRenderControls / "Batching.png";
	bRenderControlsVerified = true;
	Log(ELogLevel::Info, "Editor render controls acceptance passed");
}
} // namespace Hyperion
