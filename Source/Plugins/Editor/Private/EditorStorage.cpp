#include "EditorApplication.h"
#include "Hyperion/Config/StorageSettings.h"
#include "Hyperion/Editor/EditorPlugin.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Platform/FileDialog.h"
#include <array>

namespace Hyperion
{
std::shared_ptr<FStorageSettings> CreateEditorStorage(int InCount, char** InValues)
{
	auto Launch = ParseStorageLaunchOptions(InCount, InValues, "Editor");
	for (int Index = 1; Index < InCount; ++Index)
	{
		const std::string_view Argument(InValues[Index]);
		if (Argument == "--hidden" || Argument == "--frames" || Argument == "--benchmark" ||
		    Argument.starts_with("--exercise"))
		{
			Launch.bIsolated = true;
		}
	}
	auto Storage = std::make_shared<FStorageSettings>(std::move(Launch));
	auto Content = DefaultEngineContent(HYP_DEVELOPMENT_CONTENT);
	for (int Index = 1; Index + 1 < InCount; ++Index)
	{
		if (std::string_view(InValues[Index]) == "--engine-content")
		{
			Content = PathFromUtf8(InValues[Index + 1]);
		}
		if (std::string_view(InValues[Index]) == "--asset-root")
		{
			Storage->ProtectDirectory(PathFromUtf8(InValues[Index + 1]));
		}
	}
	Storage->ProtectDirectory(Content);
	Storage->Prepare();
	if (std::string_view(HYP_DEVELOPMENT_CONTENT).empty())
	{
		return Storage;
	}
	const auto& Paths = Storage->Paths();
	const auto Legacy = std::filesystem::path(HYP_SOURCE_DIR) / "out/editor";
	const std::array Files{std::pair{Legacy / "Preferences.ini", Paths.Config / "Preferences.ini"},
	                       std::pair{Legacy / "UiScale.ini", Paths.Config / "UiScale.ini"},
	                       std::pair{Legacy / "RenderSettings.json", Paths.Config / "RenderSettings.json"},
	                       std::pair{Legacy / "Layout.ini", Paths.State / "Layout.ini"},
	                       std::pair{Legacy / "Layout.ini.assets.ini", Paths.State / "Layout.ini.assets.ini"}};
	Storage->ImportLegacyFiles(Files);
	return Storage;
}

void FEditorPlugin::DrawStoragePreferences()
{
	auto* Storage = Context.Find<FStorageSettings>();
	if (!Storage)
	{
		Gui->TextWrapped("Storage settings are unavailable: the storage provider is disabled.");
		return;
	}
	const auto State = Storage->Get();
	if (!StorageEdit.Revision)
	{
		StorageEdit = {State.Revision, State.Saved};
	}
	Gui->Separator();
	Gui->Text("Storage locations");
	Gui->TextWrapped("Active user data: " + State.Active.UserDataRoot);
	Gui->TextWrapped("Active cache: " + State.Active.CacheRoot);
	Gui->TextWrapped("Settings file: " + State.SettingsFile);
	if (Gui->InputPathWithBrowse("User data root", StorageEdit.Roots.UserDataRoot))
	{
		const auto Directory =
		    SelectFolder(Window->Surface(), PathFromUtf8(State.Active.UserDataRoot), "Select user data root");
		if (Directory)
		{
			StorageEdit.Roots.UserDataRoot = PathToUtf8(*Directory);
		}
	}
	if (Gui->InputPathWithBrowse("Cache root", StorageEdit.Roots.CacheRoot))
	{
		const auto Directory =
		    SelectFolder(Window->Surface(), PathFromUtf8(State.Active.CacheRoot), "Select cache root");
		if (Directory)
		{
			StorageEdit.Roots.CacheRoot = PathToUtf8(*Directory);
		}
	}
	Gui->TextWrapped("Leave a field empty to restore its default. Saved changes apply after restart.");
	if (State.bUserDataOverride || State.bCacheOverride)
	{
		Gui->TextWrapped(
		    "Launch arguments/environment override " + std::string(State.bUserDataOverride ? "user data " : "") +
		    (State.bCacheOverride ? "cache " : "") + "locations. Remove those overrides to use saved choices.");
	}
	if (State.bRestartRequired)
	{
		Gui->TextWrapped("Restart required. Next user data: " + State.Next.UserDataRoot +
		                 "; cache: " + State.Next.CacheRoot);
	}
	if (Gui->Button("Save storage locations"))
	{
		try
		{
			const auto Saved = Storage->Set(StorageEdit);
			StorageEdit = {Saved.Revision, Saved.Saved};
			StorageError.clear();
		}
		catch (const std::exception& Error)
		{
			StorageError = Error.what();
		}
	}
	Gui->SameLine();
	if (Gui->Button("Reload storage choices"))
	{
		try
		{
			const auto Refreshed = Storage->Refresh();
			StorageEdit = {Refreshed.Revision, Refreshed.Saved};
			StorageError.clear();
		}
		catch (const std::exception& Error)
		{
			StorageError = Error.what();
		}
	}
	if (!StorageError.empty())
	{
		Gui->TextWrapped(StorageError);
	}
}
} // namespace Hyperion
