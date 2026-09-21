#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/IO/Path.h"
#include "Support/TestSupport.h"

namespace
{
void TestVolumeMount(const std::filesystem::path& InDirectory)
{
	using namespace Hyperion;
	FMountedFileSystem Files({{"/Volume", InDirectory.root_path(), true}});
	HYP_CHECK(Files.Normalize(InDirectory.root_path()) == "/Volume");
	HYP_CHECK(Files.Normalize(InDirectory) == std::filesystem::path("/Volume") / InDirectory.relative_path());
	const auto Rejects = [](const auto& InOperation)
	{
		try
		{
			InOperation();
		}
		catch (const std::exception&)
		{
			return true;
		}
		return false;
	};
	HYP_CHECK(Rejects(
	    [&]
	    {
		    Files.WriteAtomic(InDirectory / "Denied.bin", {});
	    }));
	HYP_CHECK(Rejects(
	    [&]
	    {
		    Files.AcquireWriteLease(InDirectory / "Denied.bin");
	    }));
	HYP_CHECK(Rejects(
	    [&]
	    {
		    FMountedFileSystem Overlap({{"/Volume", InDirectory.root_path(), true}, {"/Other", InDirectory, false}});
	    }));
}
} // namespace

void TestMountedFileSystem()
{
	using namespace Hyperion;
	const auto Root = std::filesystem::absolute("io-test/mounts");
	std::filesystem::create_directories(Root / "Engine");
	std::filesystem::create_directories(Root / "Game");
	TestVolumeMount(Root);
	FLocalFileSystem Local;
	const FBytes Bytes{std::byte{4}, std::byte{5}};
	Local.WriteAtomic(Root / "Engine/Default.bin", Bytes);
	auto Files = std::make_shared<FMountedFileSystem>(
	    std::vector<FContentMount>{{"/Engine", Root / "Engine", true}, {"/Game", Root / "Game", false}});
	FTaskSystem Tasks(1, 1);
	FIOService IO(Tasks, Files);
	const auto Path = PathFromUtf8("/Game/中文 directory/Value.bin");
	IO.WriteAsync(Path, Bytes).Get(Tasks);
	HYP_CHECK(*IO.ReadAsync(Path).Get(Tasks) == Bytes);
	HYP_CHECK(Files->Normalize(Root / "Game" / Path.relative_path().lexically_relative("Game")) == Path);
	HYP_CHECK(Files->Read("/Engine/Default.bin", 100) == Bytes);
	const std::vector<std::string_view> Extensions{".bin"};
	const auto Tree = Files->ReadTree("/Game", Extensions, 100);
	HYP_CHECK(Tree.size() == 1 && Tree.front().Path == Path && Tree.front().Bytes == Bytes);
	const std::vector<std::string_view> OtherExtensions{".hlsl"};
	HYP_CHECK(Files->ReadTree("/Game", OtherExtensions, 100).empty());
	HYP_CHECK(Files->Enumerate("/Game", true) == std::vector<std::filesystem::path>{Path});
	HYP_CHECK(Files->Enumerate("/Game", false).empty());
	HYP_CHECK(NormalizeFilePath("/Game/A/../B.bin") == "/Game/B.bin");
	const auto Fails = [](const auto& InOperation)
	{
		try
		{
			InOperation();
		}
		catch (const std::exception&)
		{
			return true;
		}
		return false;
	};
	HYP_CHECK(Fails(
	    [&]
	    {
		    Files->Read("/Missing/Value.bin", 100);
	    }));
	HYP_CHECK(Fails(
	    [&]
	    {
		    Files->Read("/Game/../Engine/Default.bin", 100);
	    }));
	HYP_CHECK(Fails(
	    [&]
	    {
		    Files->Read("/engine/Default.bin", 100);
	    }));
	HYP_CHECK(Fails(
	    [&]
	    {
		    Files->Read("/Engine/default.bin", 100);
	    }));
	HYP_CHECK(Fails(
	    [&]
	    {
		    Files->WriteAtomic("/Engine/Default.bin", Bytes);
	    }));
	HYP_CHECK(Fails(
	    [&]
	    {
		    Files->WriteAtomic(Root / "Engine/Default.bin", Bytes);
	    }));
	HYP_CHECK(Fails(
	    [&]
	    {
		    Files->AcquireWriteLease("/Engine/Default.bin");
	    }));
	HYP_CHECK(Fails(
	    [&]
	    {
		    FMountedFileSystem Duplicate({{"/Game", Root, false}, {"/Other", Root, true}});
	    }));
	{
		const auto Lease = Files->AcquireWriteLease(Path);
		HYP_CHECK(Fails(
		    [&]
		    {
			    Files->AcquireWriteLease(Path);
		    }));
	}
	std::error_code Error;
	std::filesystem::create_directory_symlink(Root / "Engine", Root / "Game/Escape", Error);
	if (!Error)
	{
		bool bLinkReported{};
		for (const auto& Entry : Files->ListDirectory("/Game"))
		{
			if (Entry.Path == "/Game/Escape")
			{
				bLinkReported = !Entry.Error.empty();
			}
		}
		HYP_CHECK(bLinkReported);
		HYP_CHECK(Fails(
		    [&]
		    {
			    Files->Read("/Game/Escape/Default.bin", 100);
		    }));
		HYP_CHECK(Fails(
		    [&]
		    {
			    Files->WriteAtomic(Root / "Game/Escape/Default.bin", Bytes);
		    }));
		std::filesystem::remove(Root / "Game/Escape");
	}
	HYP_CHECK(PathRelativeToUtf8("/Game/Asset.hasset", "/Game/Scenes") == "/Game/Asset.hasset");
#ifdef _WIN32
	HYP_CHECK(PathRelativeToUtf8("F:/Assets/Asset.hasset", "C:/Output") == "F:/Assets/Asset.hasset");
	HYP_CHECK(PathRelativeToUtf8("F:/Assets/Asset.hasset", "F:/Output") == "../Assets/Asset.hasset");
#endif
	Tasks.Shutdown();
}
