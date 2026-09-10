#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

using namespace Hyperion;

namespace
{
std::shared_ptr<FModelAsset> MakeModel()
{
	auto Model = std::make_shared<FModelAsset>();
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 0, 1, 0};
	Primitive.Indices = {0, 1, 2};
	Primitive.Material = 0;
	Model->Primitives.push_back(Primitive);
	Model->Materials.push_back({});
	FModelNode Node;
	Node.Primitives = {0};
	Model->Nodes.push_back(Node);
	Model->Roots = {0};
	return Model;
}

struct FSceneFixture
{
	FTaskSystem Tasks{2, 1};
	FIOService IO{Tasks};
	FAssetService Assets{IO};
	FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "scene-instance-shaders"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<FRenderSession> Session;
	std::atomic<bool> bRelease{false};
	std::atomic<bool> bEntered{false};

	FSceneFixture()
	{
		RegisterSceneManifestLoader(Assets);
		Assets.Register({RecordType<FModelAsset>().Id,
		                 {".model"},
		                 [this](FAssetLoadContext&)
		                 {
			                 bEntered = true;
			                 while (!bRelease)
			                 {
				                 std::this_thread::sleep_for(std::chrono::milliseconds(1));
			                 }
			                 return MakeModel();
		                 }});
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
		                          }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
		IO.WriteAsync("SceneRuntime.model", {std::byte{0}}).Get(Tasks);
		const std::string Manifest = R"({"type":"hyperion.scene","schema_version":1,
"assets":[{"id":"good","path":"SceneRuntime.model"},{"id":"bad","path":"MissingRuntime.model"}],
"instances":[{"id":"one","asset":"good"},{"id":"two","asset":"good"},{"id":"failed","asset":"bad"}],
"camera":{"eye":[0,1,7],"target":[0,0,0]}})";
		const auto Bytes = std::as_bytes(std::span(Manifest));
		IO.WriteAsync("SceneRuntime.json", {Bytes.begin(), Bytes.end()}).Get(Tasks);
	}

	~FSceneFixture()
	{
		bRelease = true;
		Assets.Drain();
		Session.reset();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device.reset();
		                          }));
	}
};

template<typename Predicate> void Await(FSceneInstance& InScene, const Predicate& InPredicate)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!InPredicate() && std::chrono::steady_clock::now() < Deadline)
	{
		InScene.Tick();
		HYP_CHECK(InScene.GetStatus().Error.empty());
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	HYP_CHECK(InPredicate());
}

void CheckLoadingEdits(FSceneFixture& InFixture)
{
	FSceneInstance Scene(*InFixture.Session, InFixture.Tasks, InFixture.Assets);

	// Release the codec before Scene unwinds even when an assertion fails.
	struct FRelease
	{
		std::atomic<bool>& bFlag;

		~FRelease()
		{
			bFlag = true;
		}
	} Release{InFixture.bRelease};

	Scene.Load("SceneRuntime.json");
	Await(Scene,
	      [&]
	      {
		      return Scene.GetModels().size() == 3 && InFixture.bEntered;
	      });
	const auto Removed = Scene.GetModels()[0].Handle;
	const auto Kept = Scene.GetModels()[1].Handle;
	const auto Failed = Scene.GetModels()[2].Handle;
	HYP_CHECK(Scene.Remove(Removed));
	const auto Added = Scene.Add({"replacement"}, "good");
	HYP_CHECK(Added.Slot == Removed.Slot && Added.Generation != Removed.Generation);
	auto Model = *Scene.Find(Kept);
	Model.World = Translation({4, 0, 0});
	Model.bVisible = false;
	HYP_CHECK(Scene.Update(Kept, Model));
	const auto Overridden = Scene.Add({"explicit data"}, "good");
	auto Explicit = *Scene.Find(Overridden);
	Explicit.Data = PrepareSceneModel(MakeModel());
	HYP_CHECK(Scene.Update(Overridden, Explicit));
	InFixture.bRelease = true;
	Await(Scene,
	      [&]
	      {
		      return Scene.GetStatus().ReadyModels == 3 && Scene.GetStatus().FailedModels == 1;
	      });
	HYP_CHECK(!Scene.Find(Removed) && !Scene.Remove(Removed));
	HYP_CHECK(Scene.Find(Kept)->World.Values[12] == 4 && !Scene.Find(Kept)->bVisible);
	HYP_CHECK(Scene.Find(Kept)->Data == Scene.Find(Added)->Data);
	HYP_CHECK(Scene.Find(Overridden)->Data == Explicit.Data);
	HYP_CHECK(Scene.Remove(Overridden));
	const auto Failure = Scene.GetError(Failed);
	HYP_CHECK(!Failure.empty());
	const auto Assets = Scene.GetAssets();
	HYP_CHECK(std::any_of(Assets.begin(), Assets.end(),
	                      [&](const auto& InAsset)
	                      {
		                      return InAsset.Id == "bad" && InAsset.Error == Failure;
	                      }));
	HYP_CHECK(Scene.GetError(Kept).empty());
	HYP_CHECK(Scene.Remove(Failed));
	const auto Reused = Scene.Add({"reused failed slot"}, "good");
	HYP_CHECK(Reused.Slot == Failed.Slot && Reused.Generation != Failed.Generation);
	HYP_CHECK(Scene.GetError(Failed).empty() && Scene.GetError(Reused).empty());
	HYP_CHECK(Scene.Remove(Reused));
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().bReady && Scene.GetStatus().Models == 2);
	Scene.Close();
	Scene.Close();
	HYP_CHECK(Scene.GetStatus().bClosed && Scene.GetModels().empty());
	Scene.Load("SceneRuntime.json");
	Await(Scene,
	      [&]
	      {
		      return Scene.GetStatus().ReadyModels == 2 && Scene.GetStatus().FailedModels == 1;
	      });
	HYP_CHECK(!Scene.Find(Kept));
}

void CheckClosePending(FSceneFixture& InFixture)
{
	InFixture.Assets.ClearCache();
	InFixture.bRelease = false;
	InFixture.bEntered = false;
	FSceneInstance Scene(*InFixture.Session, InFixture.Tasks, InFixture.Assets);
	std::jthread Release(
	    [&]
	    {
		    // An admitted external codec need not observe request cancellation itself.
		    std::this_thread::sleep_for(std::chrono::milliseconds(100));
		    InFixture.bRelease = true;
	    });
	Scene.Load("SceneRuntime.json");
	Await(Scene,
	      [&]
	      {
		      return !Scene.GetModels().empty();
	      });
	Scene.Close();
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().bClosed && Scene.GetModels().empty());
}

void CheckClosedDependencies()
{
	auto Fixture = std::make_unique<FSceneFixture>();
	auto Scene = std::make_unique<FSceneInstance>(*Fixture->Session, Fixture->Tasks, Fixture->Assets);
	Scene->Close();
	Fixture.reset();
	Scene->Close();
	Scene.reset();
}
} // namespace

int main()
{
	try
	{
		FSceneFixture Fixture;
		CheckLoadingEdits(Fixture);
		CheckClosePending(Fixture);
		CheckClosedDependencies();
		std::cout << "Independent scene loading, shared models, generation-safe edits and close passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
