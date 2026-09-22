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
	Event.Modifiers = 1;
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
	if (ExerciseStep < 200)
	{
		ExerciseAssetWindowFixture(InEvents);
		return;
	}
	if (AssetExerciseLoggedStep != static_cast<int>(ExerciseStep))
	{
		AssetExerciseLoggedStep = static_cast<int>(ExerciseStep);
		Log(ELogLevel::Info, "Asset window acceptance step " + std::to_string(ExerciseStep));
	}
	if (ExerciseStep >= 209)
	{
		ExerciseAssetWindowClosing(InEvents);
		return;
	}
	if (ExerciseStep >= 205)
	{
		ExerciseAssetWindowSizing(InEvents);
		return;
	}
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (ExerciseStep)
	{
		case 200:
			CheckAssetSaveShortcut();
			CheckFailedAssetWindow(*Window, Tasks, *Device, *Compiler, *Session, *AssetWorkspace, Control);
			CheckAssetWindow(AssetWindow && bViewportVisible && AssetWindow->RenderedFrames() > 5 &&
			                     AssetWindow->NativeWindow().Surface().Handle != Window->Surface().Handle,
			                 "Scene and asset were not rendered in separate native windows");
			CheckAssetWindow(Document && !Document->IsDirty(), "Window fixture must begin with a saved asset");
			AssetExerciseOriginalName = ReadValue<std::string>(Document->Get("name"));
			AssetExerciseSceneCamera = ViewCamera;
			WindowShortcut(InEvents, EKey::Z);
			++ExerciseStep;
			break;
		case 201:
			CheckAssetWindow(!IsDirty() && !Document->IsDirty(), "Main-window undo targeted the asset document");
			WindowShortcut(InEvents, EKey::Y);
			++ExerciseStep;
			break;
		case 202:
			CheckAssetWindow(IsDirty() && !Document->IsDirty(), "Main-window redo affected the asset document");
			WindowShortcut(InEvents, EKey::Z);
			++ExerciseStep;
			break;
		case 203:
			CheckAssetWindow(IsDirty() && HistoryCursor == 1 && Document->IsDirty() &&
			                     ViewCamera == AssetExerciseSceneCamera,
			                 "Asset-window undo affected scene history or camera");
			AssetWindow->NativeWindow().RequestClose();
			++ExerciseStep;
			break;
		case 204:
			ExerciseClick(InEvents, AssetWindow->ObservedBounds("cancel"));
			break;
	}
}

void FEditorPlugin::ExerciseAssetWindowFixture(std::vector<FInputEvent>& InEvents)
{
	switch (ExerciseStep)
	{
		case 190:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 191:
			if (++ExerciseWait == 1)
			{
				WindowShortcut(InEvents, EKey::A);
			}
			if (ExerciseWait == 3)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Window history baseline";
				InEvents.push_back(Text);
			}
			if (ExerciseWait == 6)
			{
				ExerciseWait = 0;
				++ExerciseStep;
			}
			break;
		case 192:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("canvas"));
			break;
		case 193:
			CheckAssetWindow(AssetWorkspace->IsDirty(), "Window history fixture was not edited");
			WindowShortcut(InEvents, EKey::S);
			++ExerciseStep;
			break;
		case 194:
			if (!AssetWorkspace->IsSaving() && !AssetWorkspace->IsDirty())
			{
				ExerciseStep = 200;
			}
			break;
	}
}

void FEditorPlugin::ExerciseAssetWindowSizing(std::vector<FInputEvent>&)
{
	switch (ExerciseStep)
	{
		case 205:
			CheckAssetWindow(AssetWindow && AssetWorkspace->IsDirty() && !Window->ShouldClose(),
			                 "Cancel asset-window close lost its draft or closed the main window");
			AssetWindow->NativeWindow().Resize({1100, 760});
			AssetExerciseScale = Gui->ApplicationScale();
			Gui->SetApplicationScale(1.5f);
			++ExerciseStep;
			break;
		case 206:
			if (++ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(bViewportVisible && AssetWindow->GuiContext().ApplicationScale() == 1.5f &&
			                     AssetWindow->NativeWindow().LogicalSize().Width == 1100,
			                 "Asset resize or application scaling affected the main viewport");
			Gui->SetApplicationScale(AssetExerciseScale);
			AssetExerciseFrames = AssetWindow->RenderedFrames();
			AssetWindow->NativeWindow().Minimize();
			ExerciseWait = 0;
			++ExerciseStep;
			break;
		case 207:
			if (++ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(bViewportVisible && AssetWindow->RenderedFrames() == AssetExerciseFrames,
			                 "Minimized asset window rendered or stopped the scene viewport");
			AssetWindow->NativeWindow().Restore();
			Window->Minimize();
			ExerciseWait = 0;
			++ExerciseStep;
			break;
		case 208:
			if (++ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(!bViewportVisible && AssetWindow->RenderedFrames() == AssetExerciseFrames &&
			                     AssetWorkspace->IsDirty(),
			                 "Owner minimization rendered the asset window or lost its draft");
			Window->Restore();
			ExerciseWait = 0;
			++ExerciseStep;
			Log(ELogLevel::Info, "Native window rendering, input, resize, scale and owner minimization passed");
			break;
	}
}

void FEditorPlugin::ExerciseAssetWindowClosing(std::vector<FInputEvent>& InEvents)
{
	if (ExerciseStep >= 213)
	{
		ExerciseAssetWindowSaving(InEvents);
		return;
	}
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (ExerciseStep)
	{
		case 209:
			if (++ExerciseWait < 6)
			{
				break;
			}
			CheckAssetWindow(bViewportVisible && AssetWindow->RenderedFrames() > AssetExerciseFrames + 2,
			                 "Owner restoration did not resume both windows");
			ExerciseWait = 0;
			AssetWindow->NativeWindow().RequestClose();
			++ExerciseStep;
			break;
		case 210:
			ExerciseClick(InEvents, AssetWindow->ObservedBounds("discard"));
			break;
		case 211:
		{
			CheckAssetWindow(!AssetWindow && !AssetWorkspace->HasDocuments() && IsDirty() && HistoryCursor == 1,
			                 "Discard asset window affected scene or left documents open");
			const auto Saved = Assets.LoadAsync<FSkyAsset>("/Game/Sky.hasset").Get(Tasks);
			CheckAssetWindow(Saved->Name == AssetExerciseOriginalName, "Asset window discard persisted changes");
			RequestOpenAsset("/Game/Sky.hasset");
			++ExerciseStep;
			break;
		}
		case 212:
			if (AssetWindow && Document && AssetWorkspace->IsPreviewReady())
			{
				CheckAssetWindow(!Document->IsDirty(), "Recreated asset window restored discarded edits");
				AssetWorkspace->RevealProperty("field/name");
				++ExerciseStep;
			}
			break;
	}
}

void FEditorPlugin::ExerciseAssetWindowSaving(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (ExerciseStep)
	{
		case 213:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 214:
			if (++ExerciseWait == 1)
			{
				WindowShortcut(InEvents, EKey::A);
			}
			if (ExerciseWait == 3)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Saved on window close";
				InEvents.push_back(Text);
			}
			if (ExerciseWait == 6)
			{
				AssetWindow->NativeWindow().RequestClose();
				ExerciseWait = 0;
				++ExerciseStep;
			}
			break;
		case 215:
			CheckAssetWindow(Document && Document->IsDirty(), "Window close fixture was not edited");
			if (!AssetExerciseSavedBytes)
			{
				AssetExerciseSavedBytes = IO.ReadAsync("/Game/Sky.hasset").Get(Tasks);
				IO.WriteAsync("/Game/Sky.hasset", {std::byte{0}}).Get(Tasks);
			}
			ExerciseClick(InEvents, AssetWindow->ObservedBounds("save"));
			break;
		case 216:
			CheckAssetWindow(AssetWindow && Document, "Failed save closed the asset window");
			if (!Document->IsSaving() && !Document->Error.empty())
			{
				CheckAssetWindow(Document->IsDirty() && IsDirty(), "Failed save lost document drafts");
				IO.WriteAsync("/Game/Sky.hasset", *AssetExerciseSavedBytes).Get(Tasks);
				AssetExerciseSavedBytes.reset();
				Log(ELogLevel::Info, "Failed asset-window save retained the window and dirty documents");
				++ExerciseStep;
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
				AssetExerciseIndex = 4;
				ExerciseStep = 21;
			}
			break;
	}
}
} // namespace Hyperion
