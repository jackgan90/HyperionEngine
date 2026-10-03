#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
namespace
{
void CheckAssetWindow(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

void WindowShortcut(std::vector<FInputEvent>& InEvents, EKey InKey)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.Modifiers = InputModifiers::Control;
	Event.bDown = true;
	InEvents.push_back(Event);
	Event.bDown = false;
	Event.Modifiers = 0;
	InEvents.push_back(Event);
}

void CheckFailedAssetWindow(FWindow& InOwner, FWindowGroup& InGroup, FTaskSystem& InTasks, IRHIDevice& InDevice,
                            FShaderCompiler& InCompiler, FRenderSession& InSession, FAssetWorkspace& InWorkspace,
                            FApplicationControl& InControl)
{
	FIOService MissingContent(InTasks, std::make_shared<FMemoryFileSystem>());
	bool bFailed = false;
	try
	{
		FAssetEditorWindow Host(InTasks, InDevice, InCompiler, InSession, InWorkspace, InControl, {});
		Host.Initialize(InOwner, InGroup, MissingContent, 1.25f, true);
	}
	catch (const FFileNotFound&)
	{
		bFailed = true;
	}
	CheckAssetWindow(bFailed, "Asset host unexpectedly initialized without its required font");
}
} // namespace

void FEditorAcceptanceHarness::ExerciseAssetWindowInput(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.ExerciseStep < 200)
	{
		ExerciseAssetWindowFixture(InEvents);
		return;
	}
	if (Scenario.AssetExerciseLoggedStep != static_cast<int>(Scenario.ExerciseStep))
	{
		Scenario.AssetExerciseLoggedStep = static_cast<int>(Scenario.ExerciseStep);
		Log(ELogLevel::Info, "Asset window acceptance step " + std::to_string(Scenario.ExerciseStep));
	}
	if (Scenario.ExerciseStep >= 209)
	{
		ExerciseAssetWindowClosing(InEvents);
		return;
	}
	if (Scenario.ExerciseStep >= 205)
	{
		ExerciseAssetWindowSizing(InEvents);
		return;
	}
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 200:
			CheckAssetSaveShortcut();
			CheckFailedAssetWindow(*Editor.Window, *Editor.WindowGroup, Editor.Tasks, *Editor.Device, *Editor.Compiler,
			                       *Editor.Session, *Editor.AssetWorkspace, Editor.Control);
			CheckAssetWindow(Editor.AssetWindow && Editor.Viewport.bViewportVisible &&
			                     Editor.AssetWindow->RenderedFrames() > 5 &&
			                     Editor.AssetWindow->NativeWindow().Surface().Handle != Editor.Window->Surface().Handle,
			                 "Scene and asset were not rendered in separate native windows");
			CheckAssetWindow(Document && !Document->IsDirty(), "Window fixture must begin with a saved asset");
			Scenario.AssetExerciseOriginalName = ReadValue<std::string>(Document->Get("name"));
			Scenario.AssetExerciseSceneCamera = Editor.Viewport.ViewCamera;
			WindowShortcut(InEvents, EKey::Z);
			++Scenario.ExerciseStep;
			break;
		case 201:
			if (!Editor.Scene->GetStatus().bReady)
			{
				break;
			}
			CheckAssetWindow(!Editor.IsDirty() && !Document->IsDirty(), "Main-window undo targeted the asset document");
			WindowShortcut(InEvents, EKey::Y);
			++Scenario.ExerciseStep;
			break;
		case 202:
			CheckAssetWindow(Editor.IsDirty() && !Document->IsDirty(), "Main-window redo affected the asset document");
			WindowShortcut(InEvents, EKey::Z);
			++Scenario.ExerciseStep;
			break;
		case 203:
			CheckAssetWindow(Editor.IsDirty() && Editor.HistoryCursor == 1 && Document->IsDirty() &&
			                     Editor.Viewport.ViewCamera == Scenario.AssetExerciseSceneCamera,
			                 "Asset-window undo affected scene history or camera");
			Editor.AssetWindow->NativeWindow().RequestClose();
			++Scenario.ExerciseStep;
			break;
		case 204:
			ExerciseClick(InEvents, Editor.AssetWindow->ObservedBounds("cancel"));
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetWindowFixture(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ExerciseStep)
	{
		case 190:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 191:
			if (++Scenario.ExerciseWait == 1)
			{
				WindowShortcut(InEvents, EKey::A);
			}
			if (Scenario.ExerciseWait == 3)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Window history baseline";
				InEvents.push_back(Text);
			}
			if (Scenario.ExerciseWait == 6)
			{
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		case 192:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("canvas"));
			break;
		case 193:
			CheckAssetWindow(Editor.AssetWorkspace->IsDirty(), "Window history fixture was not edited");
			WindowShortcut(InEvents, EKey::S);
			++Scenario.ExerciseStep;
			break;
		case 194:
			if (!Editor.AssetWorkspace->IsSaving() && !Editor.AssetWorkspace->IsDirty())
			{
				Scenario.ExerciseStep = 200;
			}
			break;
	}
}

void FEditorAcceptanceHarness::CheckAssetRasterOptions() const
{
	const auto View = Editor.AssetWorkspace->RenderedPreviewView();
	const auto Actual = Editor.AssetWorkspace->RenderedPreviewSettings();
	CheckAssetWindow(View && Actual && View->DepthConvention == GetDepthConvention(Editor.Rendering.bReversedZ),
	                 "Asset preview did not retain the shared depth convention");
	CheckAssetWindow(Actual->Pipeline == ESceneRenderPipeline::Deferred && Actual->GBuffer == FGBufferLayout{} &&
	                     Actual->DebugMode == 0 && !Editor.AssetWindow->LastFrameVsync(),
	                 "Main settings replaced preview defaults or the exercise-mode window VSync policy");
	bool bCheckedExposure{};
	for (const auto& Document : Editor.AssetWorkspace->Documents())
	{
		if (Document.bActive)
		{
			const auto Preview = Editor.AssetWorkspace->PreviewState(Document.Id);
			CheckAssetWindow(Preview.Settings.Exposure == Actual->Exposure,
			                 "Main exposure replaced the asset entry's independent preview exposure");
			bCheckedExposure = true;
		}
	}
	CheckAssetWindow(bCheckedExposure && Editor.Scene->GetRevision() == Scenario.AssetRasterSceneRevision &&
	                     Editor.HistoryCursor == Scenario.AssetRasterHistory,
	                 "Render setting edits changed scene history while exercising asset windows");
}

void FEditorAcceptanceHarness::BeginAssetRasterOptions()
{
	Scenario.AssetRasterInitial = Editor.Rendering;
	Scenario.AssetRasterSceneRevision = Editor.Scene->GetRevision();
	Scenario.AssetRasterHistory = Editor.HistoryCursor;
	for (const auto& Document : Editor.AssetWorkspace->Documents())
	{
		if (Document.bActive)
		{
			const auto Preview = Editor.AssetWorkspace->PreviewState(Document.Id);
			auto Settings = Preview.Settings;
			Settings.Exposure = 1.375f;
			Editor.AssetWorkspace->EditPreview(Document.Id, Preview.Generation, Settings, false);
		}
	}
	auto Candidate = Editor.Rendering;
	Candidate.bReversedZ = !Editor.Options.Rendering.bReversedZ;
	Candidate.Pipeline = "forward";
	Candidate.GBuffer = "high";
	Candidate.DebugMode = 6;
	Candidate.Exposure = 2.25f;
	Candidate.bVsync = !Candidate.bVsync;
	Editor.SetRenderSettings(Editor.RenderSettingsRevision, Candidate);
}

void FEditorAcceptanceHarness::ExerciseAssetWindowSizing(std::vector<FInputEvent>&)
{
	switch (Scenario.ExerciseStep)
	{
		case 205:
		{
			CheckAssetWindow(Editor.AssetWindow && Editor.AssetWorkspace->IsDirty() && !Editor.Window->ShouldClose(),
			                 "Cancel asset-window close lost its draft or closed the main window");
			Editor.AssetWindow->NativeWindow().Resize({1100, 760});
			Scenario.AssetExerciseScale = Editor.Gui->ApplicationScale();
			Editor.Gui->SetApplicationScale(1.5f);
			BeginAssetRasterOptions();
			++Scenario.ExerciseStep;
			break;
		}
		case 206:
			if (++Scenario.ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(Editor.Viewport.bViewportVisible &&
			                     Editor.AssetWindow->GuiContext().ApplicationScale() == 1.5f &&
			                     Editor.AssetWindow->NativeWindow().LogicalSize().Width == 1100,
			                 "Asset resize or application scaling affected the main viewport");
			Editor.Gui->SetApplicationScale(Scenario.AssetExerciseScale);
			CheckAssetWindow(Editor.AssetWorkspace->RenderedPreviewView() &&
			                     Editor.AssetWorkspace->RenderedPreviewView()->DepthConvention ==
			                         GetDepthConvention(Editor.Rendering.bReversedZ),
			                 "Open 3D asset preview did not follow live depth settings");
			CheckAssetRasterOptions();
			Scenario.AssetExerciseFrames = Editor.AssetWindow->RenderedFrames();
			Editor.AssetWindow->NativeWindow().Minimize();
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseStep;
			break;
		case 207:
		{
			if (++Scenario.ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(Editor.Viewport.bViewportVisible &&
			                     Editor.AssetWindow->RenderedFrames() == Scenario.AssetExerciseFrames,
			                 "Minimized asset window rendered or stopped the scene viewport");
			auto Candidate = Editor.Rendering;
			Candidate.bReversedZ = Editor.Options.Rendering.bReversedZ;
			Candidate.Pipeline = "deferred";
			Candidate.bVsync = !Candidate.bVsync;
			Editor.SetRenderSettings(Editor.RenderSettingsRevision, Candidate);
			Editor.AssetWindow->NativeWindow().Restore();
			Editor.Window->Minimize();
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseStep;
			break;
		}
		case 208:
			if (++Scenario.ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(!Editor.Viewport.bViewportVisible &&
			                     Editor.AssetWindow->RenderedFrames() == Scenario.AssetExerciseFrames &&
			                     Editor.AssetWorkspace->IsDirty(),
			                 "Owner minimization rendered the asset window or lost its draft");
			Editor.Window->Restore();
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseStep;
			Log(ELogLevel::Info, "Native window rendering, input, resize, scale and owner minimization passed");
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetWindowClosing(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.ExerciseStep >= 213)
	{
		ExerciseAssetWindowSaving(InEvents);
		return;
	}
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 209:
			if (++Scenario.ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(Editor.Viewport.bViewportVisible &&
			                     Editor.AssetWindow->RenderedFrames() > Scenario.AssetExerciseFrames + 2,
			                 "Owner restoration did not resume both windows");
			CheckAssetWindow(Editor.AssetWorkspace->RenderedPreviewView() &&
			                     Editor.AssetWorkspace->RenderedPreviewView()->DepthConvention ==
			                         GetDepthConvention(Editor.Rendering.bReversedZ),
			                 "Resumed asset preview retained the old depth convention");
			CheckAssetRasterOptions();
			Scenario.ExerciseWait = 0;
			Editor.AssetWindow->NativeWindow().RequestClose();
			++Scenario.ExerciseStep;
			break;
		case 210:
			ExerciseClick(InEvents, Editor.AssetWindow->ObservedBounds("discard"));
			break;
		case 211:
		{
			CheckAssetWindow(!Editor.AssetWindow && !Editor.AssetWorkspace->HasDocuments() && Editor.IsDirty() &&
			                     Editor.HistoryCursor == 1,
			                 "Discard asset window affected scene or left documents open");
			const auto Saved = Editor.Assets.LoadAsync<FSkyAsset>("/Game/Sky.hasset").Get(Editor.Tasks);
			CheckAssetWindow(Saved->Name == Scenario.AssetExerciseOriginalName,
			                 "Asset window discard persisted changes");
			Editor.RequestOpenAsset("/Game/Sky.hasset");
			++Scenario.ExerciseStep;
			break;
		}
		case 212:
			if (Editor.AssetWindow && Document && Editor.AssetWorkspace->IsPreviewReady() &&
			    Editor.AssetWorkspace->RenderedPreviewView())
			{
				CheckAssetWindow(!Document->IsDirty(), "Recreated asset window restored discarded edits");
				CheckAssetWindow(Editor.AssetWorkspace->RenderedPreviewView()->DepthConvention ==
				                     GetDepthConvention(Editor.Rendering.bReversedZ),
				                 "Reopened asset preview did not use the committed depth convention");
				CheckAssetRasterOptions();
				Editor.SetRenderSettings(Editor.RenderSettingsRevision, Scenario.AssetRasterInitial);
				Editor.AssetWorkspace->RevealProperty("field/name");
				++Scenario.ExerciseStep;
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetWindowSaving(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 213:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 214:
			if (++Scenario.ExerciseWait == 1)
			{
				WindowShortcut(InEvents, EKey::A);
			}
			if (Scenario.ExerciseWait == 3)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Saved on window close";
				InEvents.push_back(Text);
			}
			if (Scenario.ExerciseWait == 6)
			{
				Editor.AssetWindow->NativeWindow().RequestClose();
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		case 215:
			CheckAssetWindow(Document && Document->IsDirty(), "Window close fixture was not edited");
			if (!Scenario.AssetExerciseSavedBytes)
			{
				Scenario.AssetExerciseSavedBytes = Editor.IO.ReadAsync("/Game/Sky.hasset").Get(Editor.Tasks);
				Editor.IO.WriteAsync("/Game/Sky.hasset", {std::byte{0}}).Get(Editor.Tasks);
			}
			ExerciseClick(InEvents, Editor.AssetWindow->ObservedBounds("save"));
			break;
		case 216:
			CheckAssetWindow(Editor.AssetWindow && Document, "Failed save closed the asset window");
			if (!Document->IsSaving() && !Document->Error.empty())
			{
				CheckAssetWindow(Document->IsDirty() && Editor.IsDirty(), "Failed save lost document drafts");
				Editor.IO.WriteAsync("/Game/Sky.hasset", *Scenario.AssetExerciseSavedBytes).Get(Editor.Tasks);
				Scenario.AssetExerciseSavedBytes.reset();
				Log(ELogLevel::Info, "Failed asset-window save retained the window and dirty documents");
				++Scenario.ExerciseStep;
			}
			break;
		case 217:
			ExerciseClick(InEvents, Editor.AssetWindow->ObservedBounds("save"));
			break;
		case 218:
			if (!Editor.AssetWindow)
			{
				const auto Saved = Editor.Assets.LoadAsync<FSkyAsset>("/Game/Sky.hasset").Get(Editor.Tasks);
				CheckAssetWindow(Saved->Name == "Saved on window close" && Editor.IsDirty() &&
				                     Editor.HistoryCursor == 1,
				                 "Asset window save/close lost changes or affected the scene");
				Log(ELogLevel::Info, "Native asset window cancel, discard, recreation and save/close passed");
				Scenario.AssetExerciseIndex = 4;
				Scenario.ExerciseStep = 21;
			}
			break;
	}
}
} // namespace Hyperion
