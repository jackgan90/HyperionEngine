#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/SceneViewer/SceneViewerPlugin.h"
#include "Support/GraphTestSupport.h"
#include "Support/NativeAssetSupport.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>

namespace
{
using namespace Hyperion;

class FSaveFileSystem final : public IFileSystem
{
public:
	std::atomic<bool> bReleaseWrite{false};
	std::atomic<bool> bFailWrite{false};
	FNativeOnlyFileSystem Native;

	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override
	{
		return Native.Read(InPath, InLimit);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		if (InPath.filename() == "SnapshotA.hasset")
		{
			while (!bReleaseWrite)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		if (bFailWrite)
		{
			throw std::runtime_error("Injected scene save IO failure");
		}
		Native.WriteAtomic(InPath, InBytes);
	}
};

struct FViewerFixture
{
	FTaskSystem Tasks{1, 1};
	std::shared_ptr<FSaveFileSystem> Files = std::make_shared<FSaveFileSystem>();
	FIOService IO{Tasks, Files};
	FAssetService Assets{IO};
	FWindow Window{"Scene controls", {640, 480}, true};
	FShaderCompiler Compiler{TestShaderRoot(), std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	std::unique_ptr<FSceneViewerPlugin> Plugin;
	FRenderFrame Frame{{640, 480}, {}};
	FSceneVisibilityStats Statistics;
	FImage Image;

	FViewerFixture()
	{
		RegisterSceneAssetTypes(Assets.Types());
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain({Surface, {640, 480}});
		                          }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
		FLegacySceneManifest Manifest;
		Manifest.Assets = {
		    {"a",
		     {"", (std::filesystem::path(HYP_SOURCE_DIR) / "out/fixtures/native/Showcase-gltf.hasset").generic_string(),
		      RecordType<FModelAsset>().Id, ""}}};
		Manifest.Instances = {{"one", "a"}};
		Manifest.Eye = {0, 1, 7};
		Assets.SaveAsync("SceneControls.hasset", std::make_shared<const FSceneManifest>(UpgradeLegacyScene(Manifest)))
		    .Get(Tasks);
		Plugin = std::make_unique<FSceneViewerPlugin>(*Session, Tasks, Assets, "SceneControls.hasset");
		Frame.View.Width = 640;
		Frame.View.Height = 480;
		Plugin->Start();
	}

	~FViewerFixture()
	{
		Files->bReleaseWrite = true;
		Plugin->Stop();
		Plugin.reset();
		Assets.Drain();
		Session->Close();
		Session.reset();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Swapchain.reset();
			                          Device.reset();
		                          }));
	}

	void Tick()
	{
		Window.Poll();
		Frame.View.CullingViewProjection.reset();
		Plugin->Update(Frame);
		HYP_CHECK(Plugin->Error().empty());
		const auto Seed = Session->FreezeSceneFrame(Plugin->GetSceneInstance().GetToken());
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          const auto Resolved = Session->ResolveSceneFrame(*Seed, *Frame.SceneView);
			                          Frame.View = Resolved.View;
			                          FRenderGraph Graph;
			                          auto Clear = MakeColorPass(Graph, "pending");
			                          Clear.Color->Actions.Load = EAttachmentLoad::Clear;
			                          Clear.Name = "Clear";
			                          Graph.Add(Clear);
			                          if (Resolved.HasCamera())
			                          {
				                          Session->BuildViews(Graph, std::span(&Frame.View, 1), Session->FrameTargets(),
				                                              Resolved.Frame);
			                          }
			                          else
			                          {
				                          Session->BuildSceneClear(Graph, Resolved, {});
			                          }
			                          Statistics = Session->Statistics();
			                          Image = ExecuteGraph(Graph, Tasks, *Swapchain, {640, 480}, false, true);
		                          }));
		Plugin->SetRenderedView(Frame.View.Camera ? std::optional<FRenderView>{Frame.View} : std::nullopt);
	}
};

void CheckControls(FViewerFixture& InFixture)
{
	auto& Plugin = *InFixture.Plugin;
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!Plugin.Ready() && std::chrono::steady_clock::now() < Deadline)
	{
		InFixture.Tick();
	}
	HYP_CHECK(Plugin.Ready());
	const auto Original = InFixture.Image.Rgba;
	const auto Count = InFixture.Statistics.VisibleItems;
	HYP_CHECK(Count > 0);
	Plugin.ToggleSelected();
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.VisibleItems == 0 && InFixture.Image.Rgba != Original);
	Plugin.ToggleSelected();
	Plugin.MoveSelected(100);
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.CollectedPrimitives == 0);
	Plugin.MoveSelected(-100);
	InFixture.Tick();
	HYP_CHECK(InFixture.Image.Rgba == Original);
	Plugin.DuplicateSelected();
	InFixture.Tick();
	HYP_CHECK(Plugin.ModelCount() == 2 && InFixture.Statistics.Groups == 2);
	Plugin.RemoveSelected();
	InFixture.Tick();
	HYP_CHECK(Plugin.ModelCount() == 1 && InFixture.Image.Rgba == Original);
	Plugin.SetFrozen(true);
	FInputEvent Right;
	Right.Type = EEventType::Key;
	Right.Key = EKey::Right;
	Right.bDown = true;
	for (unsigned Index = 0; Index < 30; ++Index)
	{
		Plugin.Input({&Right, 1}, false, false);
		Plugin.AdvanceCamera(.1f);
	}
	Right.bDown = false;
	Plugin.Input({&Right, 1}, false, false);
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.VisibleItems == Count && InFixture.Image.Rgba != Original);
	Plugin.SetFrozen(false);
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.CollectedPrimitives == 0);
	Plugin.Fit();
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.VisibleItems == Count);
	Plugin.RemoveSelected();
	InFixture.Tick();
	HYP_CHECK(Plugin.ModelCount() == 0 && InFixture.Statistics.Groups == 0);
	Plugin.AddModel();
	InFixture.Tick();
	HYP_CHECK(Plugin.ModelCount() == 1);
}

FInputEvent CameraKey(EKey InKey, bool bInDown = true, bool bInRepeat = false)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.bDown = bInDown;
	Event.bRepeat = bInRepeat;
	return Event;
}

void CheckContinuousCamera(FViewerFixture& InFixture)
{
	auto& Scene = InFixture.Plugin->GetSceneInstance();
	const auto Handle = *GetSceneNavigationCamera(Scene);
	const auto Camera = *Scene.FindNode(Handle)->Camera;
	FSceneNodeView View;
	Scene.GetNodeView(Handle, View);
	const auto Original = View.World;
	const auto Start = SceneCameraTransform({2, 3, 9}, {0, 1, 0});
	FSceneCameraController Controller;
	const auto Pose = [&]
	{
		FSceneCameraPose Result;
		HYP_CHECK(Scene.GetCameraPose(Handle, Result));
		return Result;
	};
	const auto Simulate = [&](std::initializer_list<EKey> InKeys, unsigned InRate)
	{
		Controller.Reset();
		Scene.SetCameraView(Handle, Start, Camera);
		const auto Before = Pose();
		for (const auto Key : InKeys)
		{
			const auto Event = CameraKey(Key);
			Controller.Input(Scene, {&Event, 1}, false, false);
		}
		HYP_CHECK(Length(Subtract(Pose().Eye, Before.Eye)) == 0);
		for (unsigned Index = 0; Index < InRate; ++Index)
		{
			Controller.Input(Scene, {}, false, false);
			Controller.Advance(Scene, 1.f / InRate);
		}
		HYP_CHECK(Length(Subtract(Pose().Forward, Before.Forward)) < .0001f);
		HYP_CHECK(Scene.FindNode(Handle)->Camera->FocusDistance == Camera.FocusDistance);
		return Subtract(Pose().Eye, Before.Eye);
	};
	const auto Forward = Simulate({EKey::W}, 60);
	HYP_CHECK(Forward.Y < 0);
	HYP_CHECK(Length(Subtract(Forward, Simulate({EKey::W}, 30))) < .0001f);
	HYP_CHECK(Length(Subtract(Forward, Simulate({EKey::W}, 120))) < .0001f);
	HYP_CHECK(Length(Add(Forward, Simulate({EKey::S}, 60))) < .0001f);
	HYP_CHECK(Length(Subtract(Forward, Simulate({EKey::W, EKey::Up}, 60))) < .0001f);
	HYP_CHECK(Length(Simulate({EKey::W, EKey::S}, 60)) < .0001f);
	HYP_CHECK(std::abs(Length(Simulate({EKey::W, EKey::D, EKey::E}, 60)) - Length(Forward)) < .0001f);
	HYP_CHECK(Length(Add(Simulate({EKey::A}, 60), Simulate({EKey::D}, 60))) < .0001f);
	const auto Up = Simulate({EKey::E}, 60);
	HYP_CHECK(Up.Y > 0 && std::abs(Up.X) < .0001f && std::abs(Up.Z) < .0001f);
	HYP_CHECK(Length(Add(Up, Simulate({EKey::Q}, 60))) < .0001f);
	Controller.Reset();
	Scene.SetCameraView(Handle, Original, Camera);
	std::cout << "Reusable controller: continuous 30/60/120 Hz, pitched WASDQE, aliases and diagonal speed passed\n";
}

void CheckCameraInterruptions(FViewerFixture& InFixture)
{
	auto& Scene = InFixture.Plugin->GetSceneInstance();
	const auto Handle = *GetSceneNavigationCamera(Scene);
	const auto Camera = *Scene.FindNode(Handle)->Camera;
	FSceneNodeView View;
	Scene.GetNodeView(Handle, View);
	const auto Original = View.World;
	FSceneCameraController Controller;
	const auto Eye = [&]
	{
		FSceneCameraPose Pose;
		Scene.GetCameraPose(Handle, Pose);
		return Pose.Eye;
	};
	const auto Press = CameraKey(EKey::W);
	const auto Repeat = CameraKey(EKey::W, true, true);
	const auto Release = CameraKey(EKey::W, false);
	for (unsigned Mode = 0; Mode < 3; ++Mode)
	{
		Controller.Input(Scene, {&Press, 1}, false, false);
		Controller.Advance(Scene, .01f);
		const auto Before = Eye();
		if (Mode == 0)
		{
			Controller.Input(Scene, {&Release, 1}, false, false);
		}
		else if (Mode == 1)
		{
			Controller.Input(Scene, {}, true, true);
		}
		else
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Controller.Input(Scene, {&Focus, 1}, false, false);
			Focus.bDown = true;
			Controller.Input(Scene, {&Focus, 1}, false, false);
		}
		Controller.Input(Scene, {&Repeat, 1}, false, false);
		Controller.Advance(Scene, .1f);
		HYP_CHECK(Length(Subtract(Eye(), Before)) == 0);
	}
	Controller.Input(Scene, {&Press, 1}, false, false);
	const auto Before = Eye();
	for (const float Delta :
	     {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		Controller.Advance(Scene, Delta);
	}
	HYP_CHECK(Length(Subtract(Eye(), Before)) == 0);
	Controller.Advance(Scene, 10);
	HYP_CHECK(std::abs(Length(Subtract(Eye(), Before)) - std::max(1.f, Camera.FocusDistance) * .1f) < .0001f);
	Controller.Reset();
	const auto Stopped = Eye();
	Controller.Advance(Scene, .1f);
	HYP_CHECK(Length(Subtract(Eye(), Stopped)) == 0);
	Scene.SetEnabled(Handle, false);
	Controller.Input(Scene, {&Press, 1}, false, false);
	Controller.Advance(Scene, .1f);
	HYP_CHECK(Length(Subtract(Eye(), Stopped)) == 0);
	Scene.SetEnabled(Handle, true);
	Scene.SetCameraView(Handle, Original, Camera);
	std::cout << "Camera release, capture, focus, repeat, reset, invalid delta and stall clamp passed\n";
}

void CheckReusableMouse(FViewerFixture& InFixture)
{
	auto& Scene = InFixture.Plugin->GetSceneInstance();
	const auto Handle = *GetSceneNavigationCamera(Scene);
	const auto Camera = *Scene.FindNode(Handle)->Camera;
	FSceneNodeView View;
	Scene.GetNodeView(Handle, View);
	const auto Original = View.World;
	FSceneCameraController Controller;
	const auto Pivot = GetSceneNavigationPivot(Scene);
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.Button = 1;
	Button.bDown = true;
	Button.X = 200;
	Button.Y = 100;
	Controller.Input(Scene, {&Button, 1}, false, false);
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = 220;
	Move.Y = 110;
	Controller.Input(Scene, {&Move, 1}, false, false);
	HYP_CHECK(Length(Subtract(GetSceneNavigationPivot(Scene), Pivot)) < .0001f);
	Scene.GetNodeView(Handle, View);
	HYP_CHECK(View.World.Values != Original.Values);
	const auto Orbited = View.World;
	Controller.Input(Scene, {}, true, false);
	Move.X = 240;
	Controller.Input(Scene, {&Move, 1}, false, false);
	Scene.GetNodeView(Handle, View);
	HYP_CHECK(View.World.Values == Orbited.Values);
	FInputEvent Wheel;
	Wheel.Type = EEventType::MouseWheel;
	Wheel.Y = 1;
	Controller.Input(Scene, {&Wheel, 1}, true, false);
	HYP_CHECK(Scene.FindNode(Handle)->Camera->FocusDistance == Camera.FocusDistance);
	Controller.Input(Scene, {&Wheel, 1}, false, false);
	HYP_CHECK(std::abs(Scene.FindNode(Handle)->Camera->FocusDistance - Camera.FocusDistance * .85f) < .0001f);
	HYP_CHECK(Length(Subtract(GetSceneNavigationPivot(Scene), Pivot)) < .0001f);
	Scene.SetCameraView(Handle, Original, Camera);
}

void CheckSaveReload(FViewerFixture& InFixture)
{
	auto& Plugin = *InFixture.Plugin;
	Plugin.MoveSelected(1.25f);
	Plugin.DuplicateSelected();
	Plugin.ToggleSelected();
	const auto ReadyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	do
	{
		InFixture.Tick();
	} while (!Plugin.Ready() && std::chrono::steady_clock::now() < ReadyDeadline);
	HYP_CHECK(Plugin.Ready());
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.VisibleItems > 0);
	const auto Before = InFixture.Image.Rgba;
	const auto Output = std::filesystem::absolute("scene-save/subdirectory/edited.hasset");
	Plugin.SaveAsync(Output).Get(InFixture.Tasks);
	InFixture.Tick();
	HYP_CHECK(Plugin.SaveStatus().find("Saved scene:") == 0);
	const auto Saved = InFixture.Assets.LoadAsync<FSceneManifest>(Output).Get(InFixture.Tasks);
	HYP_CHECK(SceneModelCount(*Saved) == 2);
	const auto Models = Plugin.GetSceneInstance().GetNodes(ESceneNodeKind::Model);
	HYP_CHECK(Plugin.GetSceneInstance().FindNode(Models[0])->Id != Plugin.GetSceneInstance().FindNode(Models[1])->Id);
	HYP_CHECK(!Plugin.GetSceneInstance().FindNode(Models[1])->Model->bVisible);
	FScene Restored;
	Restored.LoadNodes(NodesFromSceneManifest(*Saved));
	FSceneCameraPose Pose;
	HYP_CHECK(Restored.GetCameraPose(Restored.FindHandle(Saved->DefaultCamera), Pose));
	HYP_CHECK(std::abs(Pose.Eye.X - InFixture.Frame.View.Eye.X) < .0001f &&
	          std::abs(Pose.Eye.Y - InFixture.Frame.View.Eye.Y) < .0001f);
	Plugin.Stop();
	InFixture.Plugin =
	    std::make_unique<FSceneViewerPlugin>(*InFixture.Session, InFixture.Tasks, InFixture.Assets, Output);
	InFixture.Plugin->Start();
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!InFixture.Plugin->Ready() && std::chrono::steady_clock::now() < Deadline)
	{
		InFixture.Tick();
	}
	HYP_CHECK(InFixture.Plugin->Ready() && InFixture.Plugin->ModelCount() == 2);
	InFixture.Tick();
	float MaximumDifference{};
	double TotalDifference{};
	std::size_t Different{};
	for (std::size_t Index = 0; Index < Before.size(); ++Index)
	{
		const auto Difference = std::abs(InFixture.Image.Rgba[Index] - Before[Index]);
		MaximumDifference = std::max(MaximumDifference, Difference);
		TotalDifference += Difference;
		Different += Difference > 1.f / 255.f;
	}
	std::cout << "Save reload pixel max=" << MaximumDifference << " mean=" << TotalDifference / Before.size()
	          << " over_one_step=" << Different << " of " << Before.size() << '\n';
	HYP_CHECK(MaximumDifference <= 1.f / 255.f);
}

void CheckSnapshotIsolationAndFailure(FViewerFixture& InFixture)
{
	auto& F = InFixture;
	auto& Plugin = *F.Plugin;
	auto& Scene = Plugin.GetSceneInstance();
	FSceneNode Rig;
	Rig.Id = "save-rig";
	Rig.Local = Translation({1, 2, 0});
	const auto Parent = Scene.AddNode(Rig);
	const auto Model = Scene.GetNodes(ESceneNodeKind::Model).front();
	Scene.Reparent(Model, Parent, ESceneReparentMode::KeepWorld);
	auto CameraNode = MakeSceneCameraNode("save-camera", {2, 1, 9}, {});
	CameraNode.Parent = "save-rig";
	const auto Camera = Scene.AddNode(CameraNode);
	const auto Light = Scene.AddNode(MakeSceneDirectionalLightNode("save-light"));
	auto Settings = Scene.GetSettings();
	Settings.DefaultCamera = Camera;
	Settings.MainDirectionalLight = Light;
	Scene.SetSettings(Settings);
	Scene.Tick();
	HYP_CHECK(Scene.GetNodes(ESceneNodeKind::Camera).size() == 2);
	HYP_CHECK(Scene.GetNodes(ESceneNodeKind::DirectionalLight).size() == 2);
	const auto Path = std::filesystem::absolute("scene-save/nodes/SnapshotA.hasset");
	const auto Expected = Serialize(Scene.Snapshot(Path));
	const auto Save = Plugin.SaveAsync(Path);
	Scene.SetCamera(Camera, {.9f, .03f, 80, 5});
	Scene.SetDirectionalLight(Light, {{.1f, .3f, .7f}, 2, false});
	Scene.SetLocalTransform(Parent, Translation({3, 0, 1}));
	Scene.Reparent(Camera, {}, ESceneReparentMode::KeepWorld);
	const auto Live = Serialize(Scene.Snapshot(Path));
	HYP_CHECK(Live != Expected);
	F.Files->bReleaseWrite = true;
	Save.Get(F.Tasks);
	F.Tick();
	const auto Loaded = F.Assets.LoadAsync<FSceneManifest>(Path).Get(F.Tasks);
	HYP_CHECK(Serialize(*Loaded) == Expected && Serialize(Scene.Snapshot(Path)) == Live);
	F.Files->bFailWrite = true;
	const auto Failed = Plugin.SaveAsync("scene-save/InjectedFailure.hasset");
	bool bRejected{};
	try
	{
		Failed.Get(F.Tasks);
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	F.Files->bFailWrite = false;
	F.Tick();
	HYP_CHECK(bRejected && Plugin.SaveStatus().find("Scene save failed:") == 0);
	HYP_CHECK(Plugin.SaveStatus().find("Injected scene save IO failure") != std::string::npos);
	HYP_CHECK(Serialize(Scene.Snapshot(Path)) == Live);
	std::cout << "Mixed hierarchy SaveAsync freezes camera/light A; live B and IO failure status remain intact\n";
}

void CheckEmptyAndNodeOnlySave(FViewerFixture& InFixture)
{
	auto& F = InFixture;
	auto& Scene = F.Plugin->GetSceneInstance();
	for (const auto Root : Scene.GetRoots())
	{
		HYP_CHECK(Scene.RemoveSubtree(Root));
	}
	for (unsigned Stage = 0; Stage < 3; ++Stage)
	{
		if (Stage == 1)
		{
			FSceneNode Group;
			Group.Id = "saved-group";
			Group.Local = Translation({2, 0, 0});
			Scene.AddNode(Group);
		}
		if (Stage == 2)
		{
			auto Camera = MakeSceneCameraNode("saved-camera", {0, 1, 6}, {});
			Camera.Parent = "saved-group";
			FSceneSettings Settings;
			Settings.DefaultCamera = Scene.AddNode(Camera);
			Settings.MainDirectionalLight = Scene.AddNode(MakeSceneDirectionalLightNode("saved-sun"));
			Settings.EnvironmentLight = Scene.AddNode(MakeSceneEnvironmentLightNode("saved-environment"));
			Scene.SetSettings(Settings);
		}
		F.Tick();
		const auto Path = std::filesystem::absolute("scene-save/NodeOnly" + std::to_string(Stage) + ".hasset");
		const auto Snapshot = Scene.Snapshot(Path);
		HYP_CHECK(Snapshot.Assets.empty() && SceneModelCount(Snapshot) == 0);
		F.Plugin->SaveAsync(Path).Get(F.Tasks);
		const auto Saved = F.Assets.LoadAsync<FSceneManifest>(Path).Get(F.Tasks);
		HYP_CHECK(Serialize(*Saved) == Serialize(Snapshot));
		Scene.Load(Path);
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		do
		{
			F.Tick();
		} while (!Scene.GetStatus().bReady && std::chrono::steady_clock::now() < Deadline);
		HYP_CHECK(Scene.GetStatus().bReady && Scene.GetStatus().Models == 0);
		HYP_CHECK(Serialize(Scene.Snapshot(Path)) == Serialize(Snapshot));
		HYP_CHECK(Scene.GetStatus().bHasActiveCamera == (Stage == 2));
	}
	std::cout << "Empty, group-only and camera/light hierarchy SaveAs reload without assets or implicit defaults\n";
}
} // namespace

int main()
{
	try
	{
		FViewerFixture Fixture;
		CheckControls(Fixture);
		CheckContinuousCamera(Fixture);
		CheckCameraInterruptions(Fixture);
		CheckReusableMouse(Fixture);
		CheckSaveReload(Fixture);
		CheckSnapshotIsolationAndFailure(Fixture);
		CheckEmptyAndNodeOnlySave(Fixture);
		std::cout
		    << "Scene Viewer visible hide/move/duplicate/remove, frozen view, fit and empty-scene recovery passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
