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
	if (Scenario.RenderControls.Progress.IsAny(
	        {ERenderControlsState::VerifyNarrowHud, ERenderControlsState::OpenProfilingCategories,
	         ERenderControlsState::SelectTasksCategory, ERenderControlsState::VerifyCategoryAndEnableFrameCollection,
	         ERenderControlsState::EnableGpuCollection, ERenderControlsState::VerifyGpuCollection,
	         ERenderControlsState::PrepareVisibilityHud, ERenderControlsState::AwaitVisibilityHud,
	         ERenderControlsState::OpenVisibilityCategories, ERenderControlsState::SelectVisibilityCategory,
	         ERenderControlsState::VerifyVisibilityCategory, ERenderControlsState::OpenBatchCategories,
	         ERenderControlsState::SelectVisibilityWithBatches, ERenderControlsState::SelectBatchCategory,
	         ERenderControlsState::VerifyVisibilityAndBatches, ERenderControlsState::VerifyBatchCounts}))
	{
		ExerciseProfilingHudInput(InEvents);
		return;
	}
	switch (Scenario.RenderControls.Progress.GetState())
	{
		case ERenderControlsState::ShowStatusHud:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["hud/status-toggle"], Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::ShowProfilingHud);
			}
			return;
		case ERenderControlsState::ShowProfilingHud:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["hud/profiling-toggle"],
			                  Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::VerifyHud);
			}
			return;
		case ERenderControlsState::VerifyHud:
			CheckRenderControlsHud();
			Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Hud.png";
			Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::OpenEditMenu);
			return;
		case ERenderControlsState::OpenEditMenu:
			if (ExerciseClick(InEvents, Scenario.EditMenuBounds, Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::OpenRenderSettings);
			}
			return;
		case ERenderControlsState::OpenRenderSettings:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["render/settings-menu"],
			                  Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::ExerciseRasterAndDepth);
			}
			return;
		case ERenderControlsState::ExerciseRasterAndDepth:
			if (Scenario.RasterOptionExercise.Case == 0 &&
			    Scenario.RasterOptionExercise.Progress.Is(ERasterState::PrepareChoice) &&
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
			Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::ExerciseViewportChoices);
			return;
		case ERenderControlsState::ExerciseViewportChoices:
			Editor.bShowRenderSettings = false;
			if (!ExerciseViewportChoices(InEvents))
			{
				return;
			}
			Editor.SceneDocument.ReplaceSelection(
			    FSceneSelection(Editor.Scene->GetLightingSelection().Directional.Handle));
			Scenario.RenderControls.Progress.TransitionTo(
			    ERenderControlsState::ExerciseLightPriorityAndExpandTransform);
			return;
		case ERenderControlsState::ExerciseLightPriorityAndExpandTransform:
			if (!ExerciseLightPriorityInput(InEvents))
			{
				return;
			}
			if (ExerciseClick(InEvents, Scenario.InspectionBounds[RecordType<FSceneTransform>().Id + "/header"],
			                  Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::EnableLightShadows);
			}
			return;
		case ERenderControlsState::EnableLightShadows:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["hyperion.scenedirectionallight/shadowSettings"],
			                  Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::VerifyLightShadows);
			}
			return;
		case ERenderControlsState::VerifyLightShadows:
			if (!Editor.Scene->FindNode(*Editor.Selection)->DirectionalLight()->ShadowSettings || !Editor.IsDirty())
			{
				throw std::runtime_error("Light shadow Inspector did not author component values");
			}
			Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Light.png";
			Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::VerifyShadowHistoryAndResize);
			return;
		case ERenderControlsState::VerifyShadowHistoryAndResize:
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
			Scenario.RenderControls.NarrowHudObservation.Restart();
			Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::VerifyNarrowHud);
			return;
	}
}

bool FEditorAcceptanceHarness::ExerciseLiveDepth(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.Depth.Progress.Is(EDepthState::FreezeCamera))
	{
		FSceneViewportOptions ViewOptions;
		ViewOptions.Frozen = true;
		Editor.SetViewportOptions(ViewOptions);
		Scenario.DepthExerciseFrozenView = *Editor.FrozenCullingView;
		Scenario.DepthExerciseCamera = Editor.Viewport.ViewCamera;
		Editor.Viewport.ViewCamera.World = Multiply(Translation({.25f, 0, 0}), Editor.Viewport.ViewCamera.World);
		Scenario.Depth.Progress.TransitionTo(EDepthState::ToggleDepth);
	}
	if (Scenario.Depth.Progress.Is(EDepthState::ToggleDepth) || Scenario.Depth.Progress.Is(EDepthState::RestoreDepth))
	{
		if (ExerciseClick(InEvents, Scenario.InspectionBounds.at("render/reversed-z"), Scenario.Depth.Click))
		{
			Scenario.Depth.Progress.TransitionTo(Scenario.Depth.Progress.Is(EDepthState::ToggleDepth)
			                                         ? EDepthState::VerifyToggledDepth
			                                         : EDepthState::VerifyRestoredDepth);
		}
		return false;
	}
	const bool bExpected = Scenario.Depth.Progress.Is(EDepthState::VerifyToggledDepth)
	                           ? !Editor.Options.Rendering.bReversedZ
	                           : Editor.Options.Rendering.bReversedZ;
	const auto State = Editor.RenderSettings();
	if (State.Values.bReversedZ != bExpected || State.bActiveReversedZ != bExpected || !Editor.FrozenCullingView ||
	    Editor.IsDirty() || !Editor.RenderStats.MainCameraView ||
	    Editor.RenderStats.MainCameraView->DepthConvention != GetDepthConvention(bExpected))
	{
		throw std::runtime_error("GUI depth toggle did not commit and render without changing the document");
	}
	const auto Expected =
	    Scenario.Depth.Progress.Is(EDepthState::VerifyToggledDepth)
	        ? Multiply(ClipDepthTransform(EDepthConvention::Reversed), Scenario.DepthExerciseFrozenView)
	        : Scenario.DepthExerciseFrozenView;
	for (std::size_t Index = 0; Index < Expected.Values.size(); ++Index)
	{
		if (std::abs(Editor.FrozenCullingView->Values[Index] - Expected.Values[Index]) > .00001f)
		{
			throw std::runtime_error("Depth switching changed the frozen camera's physical frustum");
		}
	}
	if (Scenario.Depth.Progress.Is(EDepthState::VerifyToggledDepth))
	{
		Scenario.Depth.Progress.TransitionTo(EDepthState::RestoreDepth);
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
	if (Scenario.LightPriority.Progress.Is(ELightPriorityState::Complete))
	{
		return true;
	}
	const bool bSky = DescribeLightPriorityContext(Scenario.LightPriority.Progress.GetState()).CaseIndex == 1;
	const auto Phase = DescribeLightPriorityContext(Scenario.LightPriority.Progress.GetState()).Action;
	const auto Type = bSky ? RecordType<FSceneEnvironmentLight>().Id : RecordType<FSceneDirectionalLight>().Id;
	if (Phase == ELightPriorityAction::PrepareLight && bSky)
	{
		auto Node = MakeSceneEnvironmentLightNode("priority-gui-sky");
		Node.EnvironmentLight()->Source = ESceneEnvironmentSource::ConstantColor;
		Editor.SceneDocument.ReplaceSelection(FSceneSelection(Editor.SceneDocument.CommitCreate(std::move(Node))));
	}
	if (Phase == ELightPriorityAction::FocusPriority)
	{
		if (!ExerciseClick(InEvents, Scenario.InspectionBounds.at(Type + "/priority"), Scenario.LightPriority.Click))
		{
			return false;
		}
	}
	else if ((Phase == ELightPriorityAction::PressSelectAll || Phase == ELightPriorityAction::ReleaseSelectAllAndType ||
	          Phase == ELightPriorityAction::PressEnter || Phase == ELightPriorityAction::ReleaseEnter))
	{
		if (Phase == ELightPriorityAction::PressEnter && !Scenario.LightPriority.TextCommitObservation.Advance())
		{
			return false;
		}
		Scenario.LightPriority.TextCommitObservation.Restart();
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key =
		    (Phase == ELightPriorityAction::PrepareLight || Phase == ELightPriorityAction::FocusPriority ||
		     Phase == ELightPriorityAction::PressSelectAll || Phase == ELightPriorityAction::ReleaseSelectAllAndType)
		        ? EKey::A
		        : EKey::Enter;
		Key.bDown = Phase == ELightPriorityAction::PressSelectAll || Phase == ELightPriorityAction::PressEnter;
		Key.Modifiers = Phase == ELightPriorityAction::PressSelectAll ? InputModifiers::Control : InputModifiers::None;
		InEvents.push_back(Key);
		if (Phase == ELightPriorityAction::ReleaseSelectAllAndType)
		{
			FInputEvent Text;
			Text.Type = EEventType::Text;
			Text.Text = "-7";
			InEvents.push_back(Text);
		}
	}
	else if (Phase == ELightPriorityAction::VerifyPriority)
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
	else if (Phase == ELightPriorityAction::VerifyHistory)
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
	switch (Scenario.LightPriority.Progress.GetState())
	{
		case ELightPriorityState::DirectionalPrepareLight:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::DirectionalFocusPriority);
			break;
		case ELightPriorityState::DirectionalFocusPriority:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::DirectionalPressSelectAll);
			break;
		case ELightPriorityState::DirectionalPressSelectAll:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::DirectionalReleaseSelectAllAndType);
			break;
		case ELightPriorityState::DirectionalReleaseSelectAllAndType:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::DirectionalPressEnter);
			break;
		case ELightPriorityState::DirectionalPressEnter:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::DirectionalReleaseEnter);
			break;
		case ELightPriorityState::DirectionalReleaseEnter:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::DirectionalVerifyPriority);
			break;
		case ELightPriorityState::DirectionalVerifyPriority:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::DirectionalVerifyHistory);
			break;
		case ELightPriorityState::DirectionalVerifyHistory:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::EnvironmentPrepareLight);
			break;
		case ELightPriorityState::EnvironmentPrepareLight:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::EnvironmentFocusPriority);
			break;
		case ELightPriorityState::EnvironmentFocusPriority:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::EnvironmentPressSelectAll);
			break;
		case ELightPriorityState::EnvironmentPressSelectAll:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::EnvironmentReleaseSelectAllAndType);
			break;
		case ELightPriorityState::EnvironmentReleaseSelectAllAndType:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::EnvironmentPressEnter);
			break;
		case ELightPriorityState::EnvironmentPressEnter:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::EnvironmentReleaseEnter);
			break;
		case ELightPriorityState::EnvironmentReleaseEnter:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::EnvironmentVerifyPriority);
			break;
		case ELightPriorityState::EnvironmentVerifyPriority:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::EnvironmentVerifyHistory);
			break;
		case ELightPriorityState::EnvironmentVerifyHistory:
			Scenario.LightPriority.Progress.TransitionTo(ELightPriorityState::Complete);
			break;
		default:
			break;
	}
	return false;
}

void FEditorAcceptanceHarness::ExerciseProfilingHudInput(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.RenderControls.Progress.IsAny(
	        {ERenderControlsState::PrepareVisibilityHud, ERenderControlsState::AwaitVisibilityHud,
	         ERenderControlsState::OpenVisibilityCategories, ERenderControlsState::SelectVisibilityCategory,
	         ERenderControlsState::VerifyVisibilityCategory, ERenderControlsState::OpenBatchCategories,
	         ERenderControlsState::SelectVisibilityWithBatches, ERenderControlsState::SelectBatchCategory,
	         ERenderControlsState::VerifyVisibilityAndBatches, ERenderControlsState::VerifyBatchCounts}))
	{
		ExerciseProfilingDetailsInput(InEvents);
		return;
	}
	const auto Profiling = GetProfilingStatus();
	switch (Scenario.RenderControls.Progress.GetState())
	{
		case ERenderControlsState::VerifyNarrowHud:
			if (!Scenario.RenderControls.NarrowHudObservation.Advance())
			{
				return;
			}
			CheckRenderControlsHud();
			Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Narrow.png";
			Scenario.RenderControls.NarrowHudObservation.Restart();
			Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::OpenProfilingCategories);
			return;
		case ERenderControlsState::OpenProfilingCategories:
			if (!Scenario.RenderControls.bCollectionControlsPrepared)
			{
				Scenario.RenderControls.bCollectionControlsPrepared = true;
				// The narrow HUD was checked above; collection controls use unscrolled click bounds.
				Editor.Window->Resize({1600, 960});
				Editor.Gui->SetApplicationScale(1);
			}
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["hud/categories"], Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::SelectTasksCategory);
			}
			return;
		case ERenderControlsState::SelectTasksCategory:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["hud/category/2"], Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(
				    ERenderControlsState::VerifyCategoryAndEnableFrameCollection);
			}
			return;
		case ERenderControlsState::VerifyCategoryAndEnableFrameCollection:
			if (Editor.ProfilingCategories !=
			        WithProfilingHudCategory(EProfilingHudCategory::Overview, EProfilingHudCategory::Tasks, true) ||
			    Editor.IsDirty() || Profiling.Mask)
			{
				throw std::runtime_error("Profiling category GUI changed collection or document state");
			}
			if (Profiling.bCompiled)
			{
				Editor.ChangeProfiling(ProfileCategoryMask(EProfileCategory::Frame), {});
			}
			Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::EnableGpuCollection);
			return;
		case ERenderControlsState::EnableGpuCollection:
			if (Profiling.bCompiled)
			{
				if (ExerciseClick(InEvents, Scenario.InspectionBounds["hud/collect-gpu"],
				                  Scenario.RenderControls.Click))
				{
					Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::VerifyGpuCollection);
				}
			}
			else
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::VerifyGpuCollection);
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
	Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::PrepareVisibilityHud);
}

void FEditorAcceptanceHarness::ExerciseProfilingDetailsInput(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.RenderControls.Progress.GetState())
	{
		case ERenderControlsState::PrepareVisibilityHud:
		{
			Editor.Gui->ClosePopups();
			Editor.Window->Resize({1600, 960});
			Editor.Gui->SetApplicationScale(1);
			FSceneViewportOptions ViewOptions;
			ViewOptions.ProfilingCategories = 0;
			ViewOptions.InstanceBatching = false;
			Editor.SetViewportOptions(ViewOptions);
			Scenario.RenderControls.VisibilityHudObservation.Restart();
			Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::AwaitVisibilityHud);
			return;
		}
		case ERenderControlsState::AwaitVisibilityHud:
			if (Scenario.RenderControls.VisibilityHudObservation.Advance())
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::OpenVisibilityCategories);
				Scenario.RenderControls.VisibilityHudObservation.Restart();
			}
			return;
		case ERenderControlsState::OpenVisibilityCategories:
		case ERenderControlsState::OpenBatchCategories:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["hud/categories"], Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(
				    Scenario.RenderControls.Progress.Is(ERenderControlsState::OpenVisibilityCategories)
				        ? ERenderControlsState::SelectVisibilityCategory
				        : ERenderControlsState::SelectVisibilityWithBatches);
			}
			return;
		case ERenderControlsState::SelectVisibilityCategory:
		case ERenderControlsState::SelectVisibilityWithBatches:
		{
			const auto Anchor = Scenario.InspectionBounds.at("hud/categories");
			const auto Title = Scenario.InspectionBounds.at("hud/menu-title");
			const float Padding = Editor.Gui->Scale(16);
			if (Title.X < Anchor.X || Title.X > Anchor.X + Padding || Title.Y < Anchor.W ||
			    Title.Y > Anchor.W + Padding)
			{
				throw std::runtime_error("Stats menu did not expand from the button's bottom-left corner");
			}
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["hud/category/64"], Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(
				    Scenario.RenderControls.Progress.Is(ERenderControlsState::SelectVisibilityCategory)
				        ? ERenderControlsState::VerifyVisibilityCategory
				        : ERenderControlsState::SelectBatchCategory);
			}
			return;
		}
		case ERenderControlsState::SelectBatchCategory:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds["hud/category/128"], Scenario.RenderControls.Click))
			{
				Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::VerifyVisibilityAndBatches);
			}
			return;
		case ERenderControlsState::VerifyVisibilityCategory:
			if (Editor.ProfilingCategories != EProfilingHudCategory::Visibility || Editor.IsDirty() ||
			    GetProfilingStatus().Mask)
			{
				throw std::runtime_error("Visibility category GUI did not update only the HUD state");
			}
			Editor.Gui->ClosePopups();
			Editor.HudUpdated = 0;
			Scenario.PlacementCapture = Editor.Options.ExerciseRenderControls / "Visibility.png";
			Scenario.RenderControls.Progress.TransitionTo(ERenderControlsState::OpenBatchCategories);
			return;
	}
	if (Editor.ProfilingCategories != EProfilingHudCategory::Batching || Editor.IsDirty() ||
	    GetProfilingStatus().Mask ||
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
