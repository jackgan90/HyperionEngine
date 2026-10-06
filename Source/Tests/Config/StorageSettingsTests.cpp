#include "Hyperion/Config/StorageSettings.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/IO/IOService.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Reflection/Wire.h"
#include "Support/TestSupport.h"
#include <cctype>
#include <fstream>
#include <iostream>

using namespace Hyperion;

namespace
{
template<class T> void Reject(T InOperation, std::string_view InCode = {})
{
	try
	{
		InOperation();
	}
	catch (const FStorageSettingsError& Error)
	{
		HYP_CHECK(InCode.empty() || Error.Code.GetName() == InCode);
		return;
	}
	catch (const std::exception&)
	{
		HYP_CHECK(InCode.empty());
		return;
	}
	throw std::runtime_error("Expected storage operation rejection");
}

void CheckRelocation(const std::filesystem::path& InRoot)
{
	FStorageLaunchOptions Options;
	Options.SettingsFile = InRoot / "Bootstrap/Storage.json";
	Options.UserDataRoot = InRoot / "First";
	Options.CacheRoot = InRoot / "Cache";
	{
		FStorageSettings Initial(Options);
		Initial.Prepare();
		const auto State = Initial.Get();
		const auto Saved = Initial.Set({State.Revision, State.Active});
		HYP_CHECK(Saved.bUserDataOverride && !Saved.bRestartRequired);
	}
	Options.UserDataRoot.clear();
	Options.CacheRoot.clear();
	FStorageSettings Settings(Options);
	Settings.Prepare();
	const auto Original = Settings.Get();
	auto CaseVariant = Original.Saved;
	CaseVariant.UserDataRoot[0] = static_cast<char>(std::tolower(CaseVariant.UserDataRoot[0]));
	CaseVariant.CacheRoot[0] = static_cast<char>(std::tolower(CaseVariant.CacheRoot[0]));
	const auto Unchanged = Settings.Set({Original.Revision, CaseVariant});
	HYP_CHECK(Unchanged.Revision == Original.Revision && !Unchanged.bRestartRequired);
	Reject(
	    [&]
	    {
		    Settings.Set(
		        {Original.Revision, {PathToUtf8(Settings.Paths().Application / "Nested"), Original.Active.CacheRoot}});
	    },
	    "invalid_arguments");
	HYP_CHECK(Settings.Get().Revision == Original.Revision);
	auto DynamicContent = InRoot / "GameA";
	Settings.SetProtectedDirectoryQuery(
	    [&]
	    {
		    return std::vector{DynamicContent};
	    });
	DynamicContent = InRoot / "GameB";
	Reject(
	    [&]
	    {
		    Settings.Set({Original.Revision, {Original.Active.UserDataRoot, PathToUtf8(DynamicContent)}});
	    },
	    "invalid_arguments");
	FStorageSettings Rival(Options);
	const FStorageRoots NewRoots{PathToUtf8(InRoot / "Second"), PathToUtf8(InRoot / "OtherCache")};
	const auto Pending = Settings.Set({Original.Revision, NewRoots});
	HYP_CHECK(Pending.Active == Original.Active && Pending.bRestartRequired && Pending.Next == NewRoots);
	Reject(
	    [&]
	    {
		    Settings.Set({Original.Revision, NewRoots});
	    },
	    "stale_revision");
	Reject(
	    [&]
	    {
		    Rival.Set({Original.Revision, NewRoots});
	    },
	    "stale_revision");
	std::ofstream(Settings.Paths().Config / "Preferences.ini") << "last-session-config";
	std::ofstream(Settings.Paths().State / "Layout.ini") << "saved-at-shutdown";
	std::ofstream(Settings.Paths().Logs / "old.log") << "do-not-migrate";
	{
		FStorageSettings Restarted(Options);
		Restarted.Prepare();
		HYP_CHECK(Restarted.Get().Active == NewRoots && !Restarted.Get().bRestartRequired);
		HYP_CHECK(std::filesystem::exists(Restarted.Paths().Config / "Preferences.ini"));
		HYP_CHECK(std::filesystem::exists(Restarted.Paths().State / "Layout.ini"));
		HYP_CHECK(!std::filesystem::exists(Restarted.Paths().Logs / "old.log"));
		HYP_CHECK(std::filesystem::exists(Settings.Paths().Config / "Preferences.ini"));
		const auto Current = Restarted.Get();
		Reject(
		    [&]
		    {
			    Restarted.Set({Current.Revision, {"relative/path", NewRoots.CacheRoot}});
		    },
		    "invalid_arguments");
		Reject(
		    [&]
		    {
			    Restarted.Set({Current.Revision, {NewRoots.UserDataRoot, PathToUtf8(Restarted.Paths().Application)}});
		    },
		    "invalid_arguments");
		HYP_CHECK(Restarted.Get().Revision == Current.Revision);
		Restarted.ProtectDirectory(InRoot / "Content");
		Reject(
		    [&]
		    {
			    Restarted.Set({Current.Revision, {NewRoots.UserDataRoot, PathToUtf8(InRoot / "Content")}});
		    },
		    "invalid_arguments");
	}
}

void CheckMigrationAndFailure(const std::filesystem::path& InRoot)
{
	FStorageLaunchOptions Options;
	Options.SettingsFile = InRoot / "Locator.json";
	Options.UserDataRoot = InRoot / "User";
	Options.CacheRoot = InRoot / "Cache";
	FStorageSettings Settings(Options);
	Settings.Prepare();
	const auto Legacy = InRoot / "Legacy.ini";
	std::ofstream(Legacy) << "old choice";
	const auto Target = Settings.Paths().Config / "Choice.ini";
	const std::array Files{std::pair{Legacy, Target}};
	Settings.ImportLegacyFiles(Files);
	HYP_CHECK(std::filesystem::exists(Target) && std::filesystem::exists(Legacy));
	std::filesystem::remove(Target);
	Settings.ImportLegacyFiles(Files);
	HYP_CHECK(!std::filesystem::exists(Target));
	Options.bIsolated = true;
	Options.UserDataRoot = InRoot / "Isolated";
	FStorageSettings Isolated(Options);
	Isolated.Prepare();
	const auto IsolatedTarget = Isolated.Paths().Config / "Choice.ini";
	const std::array IsolatedFiles{std::pair{Legacy, IsolatedTarget}};
	Isolated.ImportLegacyFiles(IsolatedFiles);
	HYP_CHECK(!std::filesystem::exists(IsolatedTarget));
	std::filesystem::create_directories(Options.SettingsFile);
	const auto State = Settings.Get();
	Reject(
	    [&]
	    {
		    Settings.Set({State.Revision, State.Active});
	    },
	    "save_failed");
	HYP_CHECK(Settings.Get().Saved == State.Saved);
}

void CheckWireAndIdentity()
{
	FStorageSettingsEdit Edit{7, {"D:/User", "E:/Cache"}};
	const auto Wire = WriteRecordWire(RecordType<FStorageSettingsEdit>(), &Edit);
	const auto Copy =
	    std::static_pointer_cast<FStorageSettingsEdit>(ReadRecordWire(RecordType<FStorageSettingsEdit>(), Wire));
	HYP_CHECK(Copy->Roots == Edit.Roots && Copy->Revision == Edit.Revision);
	Reject(
	    []
	    {
		    ValidateStorageIdentity("../Other");
	    });
	FStorageLaunchOptions Options;
	Options.Profile = "Experiment";
	const auto Paths = MakeApplicationPaths(Options, "D:/User", "E:/Cache");
	HYP_CHECK(Paths.Config == std::filesystem::path("D:/User/Apps/Editor/Experiment/Config"));
	HYP_CHECK(PathContains("D:/User", "d:/User/Apps"));
	HYP_CHECK(!PathContains("D:/User", "D:/Users"));
}
} // namespace

int main()
{
	try
	{
		const auto Root = std::filesystem::absolute("storage-tests") / std::to_string(ClockNanoseconds());
		std::filesystem::create_directories(Root);
		CheckWireAndIdentity();
		CheckRelocation(Root / "Relocation");
		CheckMigrationAndFailure(Root / "Migration");
		std::cout << "Storage settings tests passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
