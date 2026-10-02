#include "EditorApplication.h"
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

void CheckFailedAssetWindow(FWindow& InOwner, FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                            FRenderSession& InSession, FAssetWorkspace& InWorkspace, FApplicationControl& InControl)
{
	FIOService MissingContent(InTasks, std::make_shared<FMemoryFileSystem>());
	bool bFailed = false;
	try
	{
		FAssetEditorWindow Host(InTasks, InDevice, InCompiler, InSession, InWorkspace, InControl, {});
		Host.Initialize(InOwner, MissingContent, 1.25f, true);
	}
	catch (const FFileNotFound&)
	{
		bFailed = true;
	}
	CheckAssetWindow(bFailed, "Asset host unexpectedly initialized without its required font");
}
} // namespace

void FEditorPlugin::ExerciseAssetWindowInput(std::vector<FInputEvent>& InEvents)
{
	if (Acceptance.ExerciseStep < 200)
	{
		ExerciseAssetWindowFixture(InEvents);
		return;
	}
	if (Acceptance.AssetExerciseLoggedStep != static_cast<int>(Acceptance.ExerciseStep))
	{
		Acceptance.AssetExerciseLoggedStep = static_cast<int>(Acceptance.ExerciseStep);
		Log(ELogLevel::Info, "Asset window acceptance step " + std::to_string(Acceptance.ExerciseStep));
	}
	if (Acceptance.ExerciseStep >= 209)
	{
		ExerciseAssetWindowClosing(InEvents);
		return;
	}
	if (Acceptance.ExerciseStep >= 205)
	{
		ExerciseAssetWindowSizing(InEvents);
		return;
	}
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (Acceptance.ExerciseStep)
	{
		case 200:
			CheckAssetSaveShortcut();
			CheckFailedAssetWindow(*Window, Tasks, *Device, *Compiler, *Session, *AssetWorkspace, Control);
			CheckAssetWindow(AssetWindow && Viewport.bViewportVisible && AssetWindow->RenderedFrames() > 5 &&
			                     AssetWindow->NativeWindow().Surface().Handle != Window->Surface().Handle,
			                 "Scene and asset were not rendered in separate native windows");
			CheckAssetWindow(Document && !Document->IsDirty(), "Window fixture must begin with a saved asset");
			Acceptance.AssetExerciseOriginalName = ReadValue<std::string>(Document->Get("name"));
			Acceptance.AssetExerciseSceneCamera = Viewport.ViewCamera;
			WindowShortcut(InEvents, EKey::Z);
			++Acceptance.ExerciseStep;
			break;
		case 201:
			if (!Scene->GetStatus().bReady)
			{
				break;
			}
			CheckAssetWindow(!IsDirty() && !Document->IsDirty(), "Main-window undo targeted the asset document");
			WindowShortcut(InEvents, EKey::Y);
			++Acceptance.ExerciseStep;
			break;
		case 202:
			CheckAssetWindow(IsDirty() && !Document->IsDirty(), "Main-window redo affected the asset document");
			WindowShortcut(InEvents, EKey::Z);
			++Acceptance.ExerciseStep;
			break;
		case 203:
			CheckAssetWindow(IsDirty() && HistoryCursor == 1 && Document->IsDirty() &&
			                     Viewport.ViewCamera == Acceptance.AssetExerciseSceneCamera,
			                 "Asset-window undo affected scene history or camera");
			AssetWindow->NativeWindow().RequestClose();
			++Acceptance.ExerciseStep;
			break;
		case 204:
			ExerciseClick(InEvents, AssetWindow->ObservedBounds("cancel"));
			break;
	}
}

void FEditorPlugin::ExerciseAssetWindowFixture(std::vector<FInputEvent>& InEvents)
{
	switch (Acceptance.ExerciseStep)
	{
		case 190:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 191:
			if (++Acceptance.ExerciseWait == 1)
			{
				WindowShortcut(InEvents, EKey::A);
			}
			if (Acceptance.ExerciseWait == 3)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Window history baseline";
				InEvents.push_back(Text);
			}
			if (Acceptance.ExerciseWait == 6)
			{
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
			}
			break;
		case 192:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("canvas"));
			break;
		case 193:
			CheckAssetWindow(AssetWorkspace->IsDirty(), "Window history fixture was not edited");
			WindowShortcut(InEvents, EKey::S);
			++Acceptance.ExerciseStep;
			break;
		case 194:
			if (!AssetWorkspace->IsSaving() && !AssetWorkspace->IsDirty())
			{
				Acceptance.ExerciseStep = 200;
			}
			break;
	}
}

void FEditorPlugin::CheckAssetRasterOptions() const
{
	const auto View = AssetWorkspace->RenderedPreviewView();
	const auto Actual = AssetWorkspace->RenderedPreviewSettings();
	CheckAssetWindow(View && Actual && View->DepthConvention == GetDepthConvention(Rendering.bReversedZ),
	                 "Asset preview did not retain the shared depth convention");
	CheckAssetWindow(Actual->Pipeline == ESceneRenderPipeline::Deferred && Actual->GBuffer == FGBufferLayout{} &&
	                     Actual->DebugMode == 0 && !AssetWindow->LastFrameVsync(),
	                 "Main settings replaced preview defaults or the exercise-mode window VSync policy");
	bool bCheckedExposure{};
	for (const auto& Document : AssetWorkspace->Documents())
	{
		if (Document.bActive)
		{
			const auto Preview = AssetWorkspace->PreviewState(Document.Id);
			CheckAssetWindow(Preview.Settings.Exposure == Actual->Exposure,
			                 "Main exposure replaced the asset entry's independent preview exposure");
			bCheckedExposure = true;
		}
	}
	CheckAssetWindow(bCheckedExposure && Scene->GetRevision() == Acceptance.AssetRasterSceneRevision &&
	                     HistoryCursor == Acceptance.AssetRasterHistory,
	                 "Render setting edits changed scene history while exercising asset windows");
}

void FEditorPlugin::BeginAssetRasterOptions()
{
	Acceptance.AssetRasterInitial = Rendering;
	Acceptance.AssetRasterSceneRevision = Scene->GetRevision();
	Acceptance.AssetRasterHistory = HistoryCursor;
	for (const auto& Document : AssetWorkspace->Documents())
	{
		if (Document.bActive)
		{
			const auto Preview = AssetWorkspace->PreviewState(Document.Id);
			auto Settings = Preview.Settings;
			Settings.Exposure = 1.375f;
			AssetWorkspace->EditPreview(Document.Id, Preview.Generation, Settings, false);
		}
	}
	auto Candidate = Rendering;
	Candidate.bReversedZ = !Options.Rendering.bReversedZ;
	Candidate.Pipeline = "forward";
	Candidate.GBuffer = "high";
	Candidate.DebugMode = 6;
	Candidate.Exposure = 2.25f;
	Candidate.bVsync = !Candidate.bVsync;
	SetRenderSettings(RenderSettingsRevision, Candidate);
}

void FEditorPlugin::ExerciseAssetWindowSizing(std::vector<FInputEvent>&)
{
	switch (Acceptance.ExerciseStep)
	{
		case 205:
		{
			CheckAssetWindow(AssetWindow && AssetWorkspace->IsDirty() && !Window->ShouldClose(),
			                 "Cancel asset-window close lost its draft or closed the main window");
			AssetWindow->NativeWindow().Resize({1100, 760});
			Acceptance.AssetExerciseScale = Gui->ApplicationScale();
			Gui->SetApplicationScale(1.5f);
			BeginAssetRasterOptions();
			++Acceptance.ExerciseStep;
			break;
		}
		case 206:
			if (++Acceptance.ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(Viewport.bViewportVisible && AssetWindow->GuiContext().ApplicationScale() == 1.5f &&
			                     AssetWindow->NativeWindow().LogicalSize().Width == 1100,
			                 "Asset resize or application scaling affected the main viewport");
			Gui->SetApplicationScale(Acceptance.AssetExerciseScale);
			CheckAssetWindow(AssetWorkspace->RenderedPreviewView() &&
			                     AssetWorkspace->RenderedPreviewView()->DepthConvention ==
			                         GetDepthConvention(Rendering.bReversedZ),
			                 "Open 3D asset preview did not follow live depth settings");
			CheckAssetRasterOptions();
			Acceptance.AssetExerciseFrames = AssetWindow->RenderedFrames();
			AssetWindow->NativeWindow().Minimize();
			Acceptance.ExerciseWait = 0;
			++Acceptance.ExerciseStep;
			break;
		case 207:
		{
			if (++Acceptance.ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(Viewport.bViewportVisible &&
			                     AssetWindow->RenderedFrames() == Acceptance.AssetExerciseFrames,
			                 "Minimized asset window rendered or stopped the scene viewport");
			auto Candidate = Rendering;
			Candidate.bReversedZ = Options.Rendering.bReversedZ;
			Candidate.Pipeline = "deferred";
			Candidate.bVsync = !Candidate.bVsync;
			SetRenderSettings(RenderSettingsRevision, Candidate);
			AssetWindow->NativeWindow().Restore();
			Window->Minimize();
			Acceptance.ExerciseWait = 0;
			++Acceptance.ExerciseStep;
			break;
		}
		case 208:
			if (++Acceptance.ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(!Viewport.bViewportVisible &&
			                     AssetWindow->RenderedFrames() == Acceptance.AssetExerciseFrames &&
			                     AssetWorkspace->IsDirty(),
			                 "Owner minimization rendered the asset window or lost its draft");
			Window->Restore();
			Acceptance.ExerciseWait = 0;
			++Acceptance.ExerciseStep;
			Log(ELogLevel::Info, "Native window rendering, input, resize, scale and owner minimization passed");
			break;
	}
}

void FEditorPlugin::ExerciseAssetWindowClosing(std::vector<FInputEvent>& InEvents)
{
	if (Acceptance.ExerciseStep >= 213)
	{
		ExerciseAssetWindowSaving(InEvents);
		return;
	}
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (Acceptance.ExerciseStep)
	{
		case 209:
			if (++Acceptance.ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(Viewport.bViewportVisible &&
			                     AssetWindow->RenderedFrames() > Acceptance.AssetExerciseFrames + 2,
			                 "Owner restoration did not resume both windows");
			CheckAssetWindow(AssetWorkspace->RenderedPreviewView() &&
			                     AssetWorkspace->RenderedPreviewView()->DepthConvention ==
			                         GetDepthConvention(Rendering.bReversedZ),
			                 "Resumed asset preview retained the old depth convention");
			CheckAssetRasterOptions();
			Acceptance.ExerciseWait = 0;
			AssetWindow->NativeWindow().RequestClose();
			++Acceptance.ExerciseStep;
			break;
		case 210:
			ExerciseClick(InEvents, AssetWindow->ObservedBounds("discard"));
			break;
		case 211:
		{
			CheckAssetWindow(!AssetWindow && !AssetWorkspace->HasDocuments() && IsDirty() && HistoryCursor == 1,
			                 "Discard asset window affected scene or left documents open");
			const auto Saved = Assets.LoadAsync<FSkyAsset>("/Game/Sky.hasset").Get(Tasks);
			CheckAssetWindow(Saved->Name == Acceptance.AssetExerciseOriginalName,
			                 "Asset window discard persisted changes");
			RequestOpenAsset("/Game/Sky.hasset");
			++Acceptance.ExerciseStep;
			break;
		}
		case 212:
			if (AssetWindow && Document && AssetWorkspace->IsPreviewReady() && AssetWorkspace->RenderedPreviewView())
			{
				CheckAssetWindow(!Document->IsDirty(), "Recreated asset window restored discarded edits");
				CheckAssetWindow(AssetWorkspace->RenderedPreviewView()->DepthConvention ==
				                     GetDepthConvention(Rendering.bReversedZ),
				                 "Reopened asset preview did not use the committed depth convention");
				CheckAssetRasterOptions();
				SetRenderSettings(RenderSettingsRevision, Acceptance.AssetRasterInitial);
				AssetWorkspace->RevealProperty("field/name");
				++Acceptance.ExerciseStep;
			}
			break;
	}
}

void FEditorPlugin::ExerciseAssetWindowSaving(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (Acceptance.ExerciseStep)
	{
		case 213:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 214:
			if (++Acceptance.ExerciseWait == 1)
			{
				WindowShortcut(InEvents, EKey::A);
			}
			if (Acceptance.ExerciseWait == 3)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Saved on window close";
				InEvents.push_back(Text);
			}
			if (Acceptance.ExerciseWait == 6)
			{
				AssetWindow->NativeWindow().RequestClose();
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
			}
			break;
		case 215:
			CheckAssetWindow(Document && Document->IsDirty(), "Window close fixture was not edited");
			if (!Acceptance.AssetExerciseSavedBytes)
			{
				Acceptance.AssetExerciseSavedBytes = IO.ReadAsync("/Game/Sky.hasset").Get(Tasks);
				IO.WriteAsync("/Game/Sky.hasset", {std::byte{0}}).Get(Tasks);
			}
			ExerciseClick(InEvents, AssetWindow->ObservedBounds("save"));
			break;
		case 216:
			CheckAssetWindow(AssetWindow && Document, "Failed save closed the asset window");
			if (!Document->IsSaving() && !Document->Error.empty())
			{
				CheckAssetWindow(Document->IsDirty() && IsDirty(), "Failed save lost document drafts");
				IO.WriteAsync("/Game/Sky.hasset", *Acceptance.AssetExerciseSavedBytes).Get(Tasks);
				Acceptance.AssetExerciseSavedBytes.reset();
				Log(ELogLevel::Info, "Failed asset-window save retained the window and dirty documents");
				++Acceptance.ExerciseStep;
			}
			break;
		case 217:
			ExerciseClick(InEvents, AssetWindow->ObservedBounds("save"));
			break;
		case 218:
			if (!AssetWindow)
			{
				const auto Saved = Assets.LoadAsync<FSkyAsset>("/Game/Sky.hasset").Get(Tasks);
				CheckAssetWindow(Saved->Name == "Saved on window close" && IsDirty() && HistoryCursor == 1,
				                 "Asset window save/close lost changes or affected the scene");
				Log(ELogLevel::Info, "Native asset window cancel, discard, recreation and save/close passed");
				Acceptance.AssetExerciseIndex = 4;
				Acceptance.ExerciseStep = 21;
			}
			break;
	}
}
} // namespace Hyperion
