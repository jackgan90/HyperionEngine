#include "ContentBrowser.h"
#include "EditorPreferences.h"
#include "Hyperion/ApplicationServices/ContentRootService.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Hyperion/Textures/TextureAsset.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <source_location>

using namespace Hyperion;

namespace
{
void Check(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error("Content browser check failed at " + std::to_string(InLocation.line()));
	}
}

void WriteFixture(const std::filesystem::path& InRoot, const std::string& InName)
{
	FLocalFileSystem Files;
	for (const auto* Directory : {"Empty", "Other", ".assets", ".cache", ".git"})
	{
		std::filesystem::create_directories(InRoot / Directory);
	}
	FTextureAsset Texture{InName, EMaterialTextureEncoding::Linear, {{1, 1, {255, 255, 255, 255}}}};
	const auto Encoded = EncodeAsset(RecordType<FTextureAsset>(), &Texture);
	Files.WriteAtomic(InRoot / "Texture.hasset", Encoded.Bytes);
	FSceneManifest Scene;
	FSceneNodeEntry Model;
	Model.Id = "model";
	Model.Name = InName;
	Model.Model = FSceneNodeModel{"model"};
	Scene.Nodes.push_back(Model);
	FSceneNodeEntry Camera;
	Camera.Id = "camera";
	Camera.Camera = FSceneCamera{};
	Camera.Transform = SceneCameraTransform({0, 2, 7}, {});
	Scene.Nodes.push_back(Camera);
	Scene.DefaultCamera = "camera";
	Scene.Assets.push_back({"model", {{}, "/Game/Model.hasset", RecordType<FModelAsset>().Id, {}}});
	const auto SceneBytes = EncodeAsset(RecordType<FSceneManifest>(), &Scene);
	if (std::filesystem::exists(InRoot / "Other/Scene.hasset"))
	{
		std::filesystem::permissions(InRoot / "Other/Scene.hasset", std::filesystem::perms::owner_all);
	}
	Files.WriteAtomic(InRoot / "Scene.hasset", SceneBytes.Bytes);
	Files.WriteAtomic(InRoot / "Other" / "Scene.hasset", EncodeAsset(RecordType<FSceneManifest>(), &Scene).Bytes);
	Files.WriteAtomic(InRoot / ".assets" / "Internal.hasset", EncodeAsset(RecordType<FSceneManifest>(), &Scene).Bytes);
	Files.WriteAtomic(InRoot / ".cache" / "Cache.hasset", SceneBytes.Bytes);
	Files.WriteAtomic(InRoot / ".git" / "Git.hasset", SceneBytes.Bytes);
	const auto Primitive = std::filesystem::path(HYP_SOURCE_DIR) / "Content/Models/Primitives" /
	                       (InName == "A" ? "Cube.hasset" : "Sphere.hasset");
	const auto NativeModel = ReadValue<FModelAsset>(DecodeAsset(Files.Read(Primitive, 16 * 1024 * 1024)).Object);
	Files.WriteAtomic(InRoot / "Model.hasset", EncodeAsset(RecordType<FModelAsset>(), &NativeModel).Bytes);
	std::ofstream(InRoot / "Ignored.json") << "{}";
	std::ofstream(InRoot / "Broken.hasset") << "not a native asset";
}

void CheckBrowser(FIOService& InIO)
{
	const auto Listing = ReadContentDirectory(*InIO.FileSystem(), "/Game");
	Check(Listing.Error.empty());
	Check(Listing.Entries.size() == 7);
	Check(Listing.Entries.front().bDirectory && Listing.Entries.front().Path == "/Game/.assets");
	Check(ReadContentDirectory(*InIO.FileSystem(), "/Game/Empty").Entries.empty());
	const auto Scenes = DiscoverContentScenes(InIO);
	Check(Scenes.Paths ==
	      std::vector<std::string>{"/Game/.assets/Internal.hasset", "/Game/Other/Scene.hasset", "/Game/Scene.hasset"});
	Check(Scenes.Error.find("Broken.hasset") != std::string::npos);
	const auto Internal = DiscoverContentScenes(InIO);
	Check(Internal.Paths.size() == 3);
	Check(ReadContentHeader(InIO, "/Game/Texture.hasset").TypeId == "hyperion.textureasset");
	FContentBrowser Browser(InIO);
	Browser.Refresh(true);
	(void)Browser.Directory("/Game");
	Browser.Refresh(true);
	Browser.Stop();
}

void CheckRoots(FTaskSystem& InTasks, const std::filesystem::path& InRoot)
{
	auto Files = std::make_shared<FMountedFileSystem>(std::vector<FContentMount>{
	    {"/Engine", std::filesystem::path(HYP_SOURCE_DIR) / "Content", true}, {"/Game", InRoot / "A", false}});
	FIOService IO(InTasks, Files);
	FAssetService Assets(IO);
	RegisterSceneAssetTypes(Assets.Types());
	FContentRootService Roots(InTasks, *Files, Assets);
	CheckBrowser(IO);
	const auto Original = Assets.LoadAsync<FTextureAsset>("/Game/Texture.hasset").Get(InTasks);
	Check(Original->Name == "A");
	std::vector<FAssetRef> Entries;
	Entries.push_back({"00000000000000000000000000000042", "/Game/Texture.hasset", "hyperion.textureasset", {}});
	Assets.AddAssetIndex(Entries, "/Game");
	auto OldSave = Assets.SaveAsync("/Game/Saved.hasset", Original);
	Roots.Commit(Roots.Prepare(InRoot / "B"));
	Check(*OldSave.Get(InTasks));
	Check(std::filesystem::exists(InRoot / "A/Saved.hasset") && !std::filesystem::exists(InRoot / "B/Saved.hasset"));
	Check(Assets.LoadAsync<FTextureAsset>("/Game/Texture.hasset").Get(InTasks)->Name == "B");
	bool bRejected{};
	try
	{
		(void)Assets.Resolve({Entries.front().Id, {}, "hyperion.textureasset", {}}, {});
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	Check(bRejected);
	Roots.Commit(Roots.Prepare(InRoot / "A"));
	Check(Assets.LoadAsync<FTextureAsset>("/Game/Texture.hasset").Get(InTasks)->Name == "A");
	for (const auto& Invalid : {InRoot / "missing", std::filesystem::path(HYP_SOURCE_DIR) / "Content"})
	{
		bRejected = false;
		try
		{
			(void)Roots.Prepare(Invalid);
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		Check(bRejected && Roots.Directory() == std::filesystem::canonical(InRoot / "A"));
	}
	FEditorPreferences Preferences;
	for (int Index = 0; Index < 7; ++Index)
	{
		RememberAssetRoot(Preferences, InRoot / std::to_string(Index));
	}
	RememberAssetRoot(Preferences, InRoot / "3");
	Check(Preferences.RecentRoots.size() == 5 && Preferences.RecentRoots.front() == InRoot / "3");
	SaveEditorPreferences(InRoot / "Preferences.ini", Preferences);
	const auto Reloaded = LoadEditorPreferences(InRoot / "Preferences.ini");
	Check(Reloaded.AssetRoot == Preferences.AssetRoot && Reloaded.RecentRoots == Preferences.RecentRoots);
}
} // namespace

int main()
{
	try
	{
		FTaskSystem Tasks(4, 1);
		const auto Root = std::filesystem::current_path() / "editor-content-tests";
		WriteFixture(Root / "A", "A");
		WriteFixture(Root / "B", "B");
		std::filesystem::remove(Root / "A/Saved.hasset");
		std::filesystem::remove(Root / "B/Saved.hasset");
		CheckRoots(Tasks, Root);
		std::cout
		    << "Content discovery, filtering, cancellation, root isolation, old save and recent persistence passed\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
