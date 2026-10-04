#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <cmath>

namespace Hyperion
{
void FEditorAcceptanceHarness::CheckRenderControlsHud() const
{
	const auto Clip = Editor.Viewport.ViewportRegion.Bounds;
	for (const auto* Name : {"hud/status", "hud/profiling"})
	{
		const auto Bounds = Scenario.InspectionBounds.at(Name);
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
	if (Scenario.InspectionBounds.at("hud/status").Z > Scenario.InspectionBounds.at("hud/profiling").X)
	{
		throw std::runtime_error("Status and profiling HUDs overlap");
	}
	for (const auto* Name :
	     {"hud/status-toggle", "hud/profiling-toggle", "hud/visualizer", "hud/exposure", "hud/categories"})
	{
		const auto Bounds = Scenario.InspectionBounds.at(Name);
		if (Bounds.Z <= Bounds.X || Bounds.X < Clip.X || Bounds.Z > Clip.Z || Bounds.W > Clip.Y)
		{
			throw std::runtime_error("Render toolbar control is clipped: " + std::string(Name));
		}
	}
	if (Editor.IsDirty())
	{
		throw std::runtime_error("HUD controls changed the scene document");
	}
}

void FEditorAcceptanceHarness::ExerciseRenderControlsInput(std::vector<FInputEvent>& InEvents)
{
	if (Editor.ReadyFrames < 8)
	{
		return;
	}
	if (Scenario.ExerciseStep >= 11)
	{
		ExerciseProfilingHudInput(InEvents);
		return;
	}
	switch (Scenario.ExerciseStep)
	{
		case 0:
			ExerciseClick(InEvents, Scenario.InspectionBounds["hud/status-toggle"]);
			return;
		case 1:
			ExerciseClick(InEvents, Scenario.InspectionBounds["hud/profiling-toggle"]);
			return;
		case 2:
			CheckRenderControlsHud();
			Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Hud.png";
			++Scenario.ExerciseStep;
			return;
		case 3:
			ExerciseClick(InEvents, Scenario.EditMenuBounds);
			return;
		case 4:
			ExerciseClick(InEvents, Scenario.InspectionBounds["render/settings-menu"]);
			return;
		case 5:
			if (Scenario.RasterOptionExercise.Case == 0 && Scenario.RasterOptionExercise.Step == 0 &&
			    (!Editor.bShowRenderSettings ||
			     Scenario.InspectionBounds["render/pipeline"].Z <= Scenario.InspectionBounds["render/pipeline"].X))
			{
				throw std::runtime_error("Render settings menu did not open its window");
			}
			if (!ExerciseRasterOptions(InEvents) || !ExerciseLiveDepth(InEvents))
			{
				return;
			}
			Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Settings.png";
			++Scenario.ExerciseStep;
			return;
		case 6:
			Editor.bShowRenderSettings = false;
			if (!ExerciseViewportChoices(InEvents))
			{
				return;
			}
			Editor.SceneDocument.ReplaceSelection(
			    FSceneSelection(Editor.Scene->GetLightingSelection().Directional.Handle));
			++Scenario.ExerciseStep;
			return;
		case 7:
			if (!ExerciseLightPriorityInput(InEvents))
			{
				return;
			}
			ExerciseClick(InEvents, Scenario.InspectionBounds[RecordType<FSceneTransform>().Id + "/header"]);
			return;
		case 8:
			ExerciseClick(InEvents, Scenario.InspectionBounds["hyperion.scenedirectionallight/shadowSettings"]);
			return;
		case 9:
			if (!Editor.Scene->FindNode(*Editor.Selection)->DirectionalLight()->ShadowSettings || !Editor.IsDirty())
			{
				throw std::runtime_error("Light shadow Inspector did not author component values");
			}
			Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Light.png";
			++Scenario.ExerciseStep;
			return;
		case 10:
			Editor.Undo();
			if (Editor.Scene->FindNode(*Editor.Selection)->DirectionalLight()->ShadowSettings)
			{
				throw std::runtime_error("Shadow Inspector undo failed");
			}
			Editor.Redo();
			if (!Editor.Scene->FindNode(*Editor.Selection)->DirectionalLight()->ShadowSettings)
			{
				throw std::runtime_error("Shadow Inspector redo failed");
			}
			Editor.Undo();
			Editor.Window->Resize({1000, 900});
			Editor.Gui->SetApplicationScale(2);
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseStep;
			return;
	}
}

bool FEditorAcceptanceHarness::ExerciseLiveDepth(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.DepthExerciseStep == 0)
	{
		FSceneViewportOptions ViewOptions;
		ViewOptions.Frozen = true;
		Editor.SetViewportOptions(ViewOptions);
		Scenario.DepthExerciseFrozenView = *Editor.FrozenCullingView;
		Scenario.DepthExerciseCamera = Editor.Viewport.ViewCamera;
		Editor.Viewport.ViewCamera.World = Multiply(Translation({.25f, 0, 0}), Editor.Viewport.ViewCamera.World);
		++Scenario.DepthExerciseStep;
	}
	if (Scenario.DepthExerciseStep == 1 || Scenario.DepthExerciseStep == 3)
	{
		const auto Step = Scenario.ExerciseStep;
		ExerciseClick(InEvents, Scenario.InspectionBounds.at("render/reversed-z"));
		if (Scenario.ExerciseStep != Step)
		{
			++Scenario.DepthExerciseStep;
		}
		Scenario.ExerciseStep = Step;
		return false;
	}
	const bool bExpected =
	    Scenario.DepthExerciseStep == 2 ? !Editor.Options.Rendering.bReversedZ : Editor.Options.Rendering.bReversedZ;
	const auto State = Editor.RenderSettings();
	if (State.Values.bReversedZ != bExpected || State.bActiveReversedZ != bExpected || !Editor.FrozenCullingView ||
	    Editor.IsDirty() || !Editor.RenderStats.MainCameraView ||
	    Editor.RenderStats.MainCameraView->DepthConvention != GetDepthConvention(bExpected))
	{
		throw std::runtime_error("GUI depth toggle did not commit and render without changing the document");
	}
	const auto Expected = Scenario.DepthExerciseStep == 2 ? Multiply(ClipDepthTransform(EDepthConvention::Reversed),
	                                                                 Scenario.DepthExerciseFrozenView)
	                                                      : Scenario.DepthExerciseFrozenView;
	for (std::size_t Index = 0; Index < Expected.Values.size(); ++Index)
	{
		if (std::abs(Editor.FrozenCullingView->Values[Index] - Expected.Values[Index]) > .00001f)
		{
			throw std::runtime_error("Depth switching changed the frozen camera's physical frustum");
		}
	}
	if (Scenario.DepthExerciseStep == 2)
	{
		++Scenario.DepthExerciseStep;
		return false;
	}
	Editor.Viewport.ViewCamera = Scenario.DepthExerciseCamera;
	FSceneViewportOptions ViewOptions;
	ViewOptions.Frozen = false;
	Editor.SetViewportOptions(ViewOptions);
	Log(ELogLevel::Info, "Live GUI depth switching and frozen-camera preservation passed");
	return true;
}

bool FEditorAcceptanceHarness::ExerciseLightPriorityInput(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.LightPriorityExerciseStep >= 16)
	{
		return true;
	}
	const bool bSky = Scenario.LightPriorityExerciseStep >= 8;
	const unsigned Phase = Scenario.LightPriorityExerciseStep % 8;
	const auto Type = bSky ? RecordType<FSceneEnvironmentLight>().Id : RecordType<FSceneDirectionalLight>().Id;
	if (Phase == 0 && bSky)
	{
		auto Node = MakeSceneEnvironmentLightNode("priority-gui-sky");
		Node.EnvironmentLight()->Source = ESceneEnvironmentSource::ConstantColor;
		Editor.SceneDocument.ReplaceSelection(FSceneSelection(Editor.SceneDocument.CommitCreate(std::move(Node))));
	}
	if (Phase == 1)
	{
		const auto Step = Scenario.ExerciseStep;
		ExerciseClick(InEvents, Scenario.InspectionBounds.at(Type + "/priority"));
		if (Step == Scenario.ExerciseStep)
		{
			return false;
		}
		Scenario.ExerciseStep = Step;
	}
	else if (Phase >= 2 && Phase <= 5)
	{
		if (Phase == 4 && Scenario.ExerciseWait++ == 0)
		{
			return false;
		}
		Scenario.ExerciseWait = 0;
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = Phase < 4 ? EKey::A : EKey::Enter;
		Key.bDown = Phase == 2 || Phase == 4;
		Key.Modifiers = Phase == 2 ? InputModifiers::Control : InputModifiers::None;
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
		const auto* Node = Editor.Scene->FindNode(*Editor.Selection);
		const auto Priority = bSky ? Node->EnvironmentLight()->Priority : Node->DirectionalLight()->Priority;
		if (Priority != -7 || !Editor.IsDirty())
		{
			throw std::runtime_error("Priority Inspector did not commit a signed integer: " + Editor.Error);
		}
		Scenario.PlacementCapture =
		    Editor.Options.ExerciseRenderControls / (bSky ? "SkyPriority.png" : "DirectionalPriority.png");
	}
	else if (Phase == 7)
	{
		Editor.Undo();
		const auto* Node = Editor.Scene->FindNode(*Editor.Selection);
		if ((bSky ? Node->EnvironmentLight()->Priority : Node->DirectionalLight()->Priority) != 0)
		{
			throw std::runtime_error("Priority Inspector undo failed");
		}
		Editor.Redo();
		Node = Editor.Scene->FindNode(*Editor.Selection);
		if ((bSky ? Node->EnvironmentLight()->Priority : Node->DirectionalLight()->Priority) != -7)
		{
			throw std::runtime_error("Priority Inspector redo failed");
		}
		Editor.Undo();
		if (bSky)
		{
			Editor.Undo();
			Editor.SceneDocument.ReplaceSelection(
			    FSceneSelection(Editor.Scene->GetLightingSelection().Directional.Handle));
		}
	}
	++Scenario.LightPriorityExerciseStep;
	return false;
}

void FEditorAcceptanceHarness::ExerciseProfilingHudInput(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.ExerciseStep >= 17)
	{
		ExerciseProfilingDetailsInput(InEvents);
		return;
	}
	const auto Profiling = GetProfilingStatus();
	switch (Scenario.ExerciseStep)
	{
		case 11:
			if (++Scenario.ExerciseWait < 8)
			{
				return;
			}
			CheckRenderControlsHud();
			Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Narrow.png";
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseStep;
			return;
		case 12:
			if (!Scenario.ExerciseWait)
			{
				// The narrow HUD was checked above; collection controls use unscrolled click bounds.
				Editor.Window->Resize({1600, 960});
				Editor.Gui->SetApplicationScale(1);
			}
			ExerciseClick(InEvents, Scenario.InspectionBounds["hud/categories"]);
			return;
		case 13:
			ExerciseClick(InEvents, Scenario.InspectionBounds["hud/category/1"]);
			return;
		case 14:
			if (Editor.ProfilingCategories != 3 || Editor.IsDirty() || Profiling.Mask)
			{
				throw std::runtime_error("Profiling category GUI changed collection or document state");
			}
			if (Profiling.bCompiled)
			{
				Editor.ChangeProfiling(ProfileCategoryMask(EProfileCategory::Frame), {});
			}
			++Scenario.ExerciseStep;
			return;
		case 15:
			if (Profiling.bCompiled)
			{
				ExerciseClick(InEvents, Scenario.InspectionBounds["hud/collect-gpu"]);
			}
			else
			{
				++Scenario.ExerciseStep;
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
		Editor.ChangeProfiling(0, {});
	}
	Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Stats.png";
	++Scenario.ExerciseStep;
}

void FEditorAcceptanceHarness::ExerciseProfilingDetailsInput(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ExerciseStep)
	{
		case 17:
		{
			Editor.Gui->ClosePopups();
			Editor.Window->Resize({1600, 960});
			Editor.Gui->SetApplicationScale(1);
			FSceneViewportOptions ViewOptions;
			ViewOptions.ProfilingCategories = 0;
			ViewOptions.InstanceBatching = false;
			Editor.SetViewportOptions(ViewOptions);
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseStep;
			return;
		}
		case 18:
			if (++Scenario.ExerciseWait >= 8)
			{
				++Scenario.ExerciseStep;
				Scenario.ExerciseWait = 0;
			}
			return;
		case 19:
		case 22:
			ExerciseClick(InEvents, Scenario.InspectionBounds["hud/categories"]);
			return;
		case 20:
		case 23:
		{
			const auto Anchor = Scenario.InspectionBounds.at("hud/categories");
			const auto Title = Scenario.InspectionBounds.at("hud/menu-title");
			const float Padding = Editor.Gui->Scale(16);
			if (Title.X < Anchor.X || Title.X > Anchor.X + Padding || Title.Y < Anchor.W ||
			    Title.Y > Anchor.W + Padding)
			{
				throw std::runtime_error("Stats menu did not expand from the button's bottom-left corner");
			}
			ExerciseClick(InEvents, Scenario.InspectionBounds["hud/category/6"]);
			return;
		}
		case 24:
			ExerciseClick(InEvents, Scenario.InspectionBounds["hud/category/7"]);
			return;
		case 21:
			if (Editor.ProfilingCategories != 64 || Editor.IsDirty() || GetProfilingStatus().Mask)
			{
				throw std::runtime_error("Visibility category GUI did not update only the HUD state");
			}
			Editor.Gui->ClosePopups();
			Editor.HudUpdated = 0;
			Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Visibility.png";
			++Scenario.ExerciseStep;
			return;
	}
	if (Editor.ProfilingCategories != 128 || Editor.IsDirty() || GetProfilingStatus().Mask ||
	    !Editor.RenderStats.MainView().Batches.Fallbacks[static_cast<std::size_t>(ERenderBatchFallback::Disabled)])
	{
		throw std::runtime_error("Batching HUD requires the selected category and disabled-batching fallback data");
	}
	Editor.Gui->ClosePopups();
	Editor.HudUpdated = 0;
	Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Batching.png";
	Scenario.bRenderControlsVerified = true;
	Log(ELogLevel::Info, "Editor render controls acceptance passed");
}
} // namespace Hyperion
