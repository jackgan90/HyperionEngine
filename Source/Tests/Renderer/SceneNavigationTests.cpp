#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneEditTarget.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/Renderer/SceneViewport.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include "Hyperion/SceneEditing/ScenePlacement.h"
#include "Support/GraphTestSupport.h"
#include "Support/NativeAssetSupport.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <array>
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
	std::atomic<bool> bHoldReload{false};
	FNativeOnlyFileSystem Native;

	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override
	{
		while (InPath.filename() == "FocusReload.hasset" && bHoldReload)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
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

struct FSceneFixture
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
	std::unique_ptr<FSceneInstance> Scene;
	std::unique_ptr<FSceneInstanceEditTarget> Target;
	FSceneEditDocument Document;
	FSceneViewRequest Request;
	FSceneVisibilityStats Statistics;
	FImage Image;

	FSceneFixture()
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
		Scene = std::make_unique<FSceneInstance>(*Session, Tasks, Assets);
		Scene->Load("SceneControls.hasset");
		Target = std::make_unique<FSceneInstanceEditTarget>(*Scene, Assets);
		Document.Attach(*Target);
		Request.Width = 640;
		Request.Height = 480;
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		do
		{
			Tick();
		} while (!Scene->GetStatus().bReady && std::chrono::steady_clock::now() < Deadline);
		HYP_CHECK(Scene->GetStatus().bReady);
	}

	~FSceneFixture()
	{
		Files->bReleaseWrite = true;
		Files->bHoldReload = false;
		Document.Detach(Tasks);
		Target.reset();
		Scene.reset();
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
		Scene->Tick();
		Document.PollSave();
		HYP_CHECK(Scene->GetStatus().Error.empty());
		const auto Seed = Session->FreezeSceneFrame(Scene->GetToken());
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          const auto Resolved = Session->ResolveSceneFrame(*Seed, Request);
			                          FRenderGraph Graph;
			                          auto Clear = MakeColorPass(Graph, "pending");
			                          Clear.Color->Actions.Load = EAttachmentLoad::Clear;
			                          Clear.Name = "Clear";
			                          Graph.Add(Clear);
			                          if (Resolved.HasCamera())
			                          {
				                          Session->BuildViews(Graph, std::span(&Resolved.View, 1),
				                                              Session->FrameTargets(), Resolved.Frame);
			                          }
			                          else
			                          {
				                          Session->BuildSceneClear(Graph, Resolved, {});
			                          }
			                          Statistics = Session->Statistics();
			                          Image = ExecuteGraph(Graph, Tasks, *Swapchain, {640, 480}, false, true);
		                          }));
	}
};

FInputEvent CameraKey(EKey InKey, bool bInDown = true, bool bInRepeat = false)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.bDown = bInDown;
	Event.bRepeat = bInRepeat;
	return Event;
}

void CheckContinuousCamera(FSceneFixture& InFixture)
{
	auto& Scene = *InFixture.Scene;
	const auto Handle = *GetSceneNavigationCamera(Scene);
	const auto Camera = *Scene.FindNode(Handle)->Camera();
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
		HYP_CHECK(Scene.FindNode(Handle)->Camera()->FocusDistance == Camera.FocusDistance);
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

void CheckCameraInterruptions(FSceneFixture& InFixture)
{
	auto& Scene = *InFixture.Scene;
	const auto Handle = *GetSceneNavigationCamera(Scene);
	const auto Camera = *Scene.FindNode(Handle)->Camera();
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

void CheckReusableMouse(FSceneFixture& InFixture)
{
	auto& Scene = *InFixture.Scene;
	const auto Handle = *GetSceneNavigationCamera(Scene);
	const auto Camera = *Scene.FindNode(Handle)->Camera();
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
	HYP_CHECK(Scene.FindNode(Handle)->Camera()->FocusDistance == Camera.FocusDistance);
	Controller.Input(Scene, {&Wheel, 1}, false, false);
	HYP_CHECK(std::abs(Scene.FindNode(Handle)->Camera()->FocusDistance - Camera.FocusDistance * .85f) < .0001f);
	HYP_CHECK(Length(Subtract(GetSceneNavigationPivot(Scene), Pivot)) < .0001f);
	Scene.SetCameraView(Handle, Original, Camera);
}

FInputEvent CameraButton(bool bInDown = true)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseButton;
	Event.Button = 1;
	Event.bDown = bInDown;
	Event.X = 200;
	Event.Y = 100;
	return Event;
}

FSceneCameraPose ReadCameraPose(FSceneInstance& InScene, FSceneHandle InHandle)
{
	FSceneCameraPose Pose;
	HYP_CHECK(InScene.GetCameraPose(InHandle, Pose));
	return Pose;
}

FInputEvent CameraWheel(float InDelta)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseWheel;
	Event.Y = InDelta;
	return Event;
}

void CheckFlyWheel(FSceneInstance& InScene, FSceneHandle InHandle, const FSceneCamera& InCamera)
{
	FSceneCameraController Controller(ESceneCameraNavigationMode::Fly);
	const auto Start = SceneCameraTransform({2, 3, 9}, {3, 4, 8});
	InScene.SetCameraView(InHandle, Start, InCamera);
	const auto Before = ReadCameraPose(InScene, InHandle);
	const float InitialSpeed = Controller.GetMovementSpeed(InScene);
	const std::array Faster{CameraButton(), CameraWheel(1)};
	Controller.Input(InScene, Faster, false, false);
	const float Speed = Controller.GetMovementSpeed(InScene);
	HYP_CHECK(std::abs(Speed - InitialSpeed * 1.2f) < .0001f);
	const auto Adjusted = ReadCameraPose(InScene, InHandle);
	HYP_CHECK(Length(Subtract(Adjusted.Eye, Before.Eye)) == 0);
	HYP_CHECK(Length(Subtract(Adjusted.Forward, Before.Forward)) == 0);
	HYP_CHECK(*InScene.FindNode(InHandle)->Camera() == InCamera);
	const auto Key = CameraKey(EKey::W);
	Controller.Input(InScene, {&Key, 1}, false, false);
	Controller.Advance(InScene, .02f);
	const auto Moved = ReadCameraPose(InScene, InHandle);
	HYP_CHECK(std::abs(Length(Subtract(Moved.Eye, Before.Eye)) - Speed * .02f) < .0001f);
	HYP_CHECK(Dot(Normalize(Subtract(Moved.Eye, Before.Eye)), Before.Forward) > .9999f);
	const std::array Dolly{CameraButton(false), CameraWheel(1)};
	Controller.Input(InScene, Dolly, false, false);
	const auto Dollied = ReadCameraPose(InScene, InHandle);
	HYP_CHECK(Dot(Normalize(Subtract(Dollied.Eye, Moved.Eye)), Before.Forward) > .9999f);
	HYP_CHECK(std::abs(Length(Subtract(Dollied.Eye, Moved.Eye)) - InCamera.FocusDistance * .15f) < .0001f);
	HYP_CHECK(Controller.GetMovementSpeed(InScene) == Speed);
	Controller.Advance(InScene, .02f);
	HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Eye, Dollied.Eye)) == 0);
	const std::array Slower{CameraButton(), CameraWheel(-.5f), CameraWheel(-.5f)};
	Controller.Input(InScene, Slower, false, false);
	HYP_CHECK(std::abs(Controller.GetMovementSpeed(InScene) - InitialSpeed) < .0001f);
	HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Eye, Dollied.Eye)) == 0);
	InScene.SetCameraView(InHandle, Start, InCamera);
}

void CheckFlySpeedInterruptions(FSceneInstance& InScene, FSceneHandle InHandle)
{
	FSceneCameraController Controller(ESceneCameraNavigationMode::Fly);
	const auto Before = ReadCameraPose(InScene, InHandle);
	const std::array Faster{CameraButton(), CameraWheel(1)};
	Controller.Input(InScene, Faster, false, false);
	const float Speed = Controller.GetMovementSpeed(InScene);
	Controller.Reset();
	HYP_CHECK(Controller.GetMovementSpeed(InScene) == Speed);
	Controller.Input(InScene, Faster, true, true);
	HYP_CHECK(Controller.GetMovementSpeed(InScene) == Speed);
	FInputEvent Focus;
	Focus.Type = EEventType::Focus;
	Controller.Input(InScene, {&Focus, 1}, false, false);
	Controller.Input(InScene, Faster, false, false);
	HYP_CHECK(Controller.GetMovementSpeed(InScene) == Speed);
	Focus.bDown = true;
	Controller.Input(InScene, {&Focus, 1}, false, false);
	HYP_CHECK(Controller.GetMovementSpeed(InScene) == Speed);
	const auto Press = CameraButton();
	Controller.Input(InScene, {&Press, 1}, false, false);
	for (const float Delta : {0.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
	{
		const auto Wheel = CameraWheel(Delta);
		Controller.Input(InScene, {&Wheel, 1}, false, false);
		HYP_CHECK(Controller.GetMovementSpeed(InScene) == Speed);
	}
	for (const float Delta : {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()})
	{
		const auto Wheel = CameraWheel(Delta);
		Controller.Input(InScene, {&Wheel, 1}, false, false);
		const float Limit = Controller.GetMovementSpeed(InScene);
		HYP_CHECK(std::isfinite(Limit) && Limit > 0);
		HYP_CHECK(Delta > 0 ? Limit > Speed : Limit < Speed);
		Controller.Input(InScene, {&Wheel, 1}, false, false);
		HYP_CHECK(Controller.GetMovementSpeed(InScene) == Limit);
	}
	HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Eye, Before.Eye)) == 0);
}

void CheckFlyTranslation(FSceneInstance& InScene, FSceneHandle InHandle, const FSceneCamera& InCamera)
{
	FSceneCameraController Controller(ESceneCameraNavigationMode::Fly);
	const auto Press = CameraButton();
	const auto Release = CameraButton(false);
	const auto Start = SceneCameraTransform({2, 3, 9}, {2, 3, 8});
	const float Distance = std::max(1.f, InCamera.FocusDistance) * .02f;
	for (const auto Key : {EKey::W, EKey::A, EKey::S, EKey::D, EKey::Q, EKey::E, EKey::Up, EKey::Left, EKey::Down,
	                       EKey::Right, EKey::PageDown, EKey::PageUp})
	{
		Controller.Reset();
		InScene.SetCameraView(InHandle, Start, InCamera);
		const auto Before = ReadCameraPose(InScene, InHandle);
		const auto KeyDown = CameraKey(Key);
		Controller.Input(InScene, {&KeyDown, 1}, false, false);
		Controller.Advance(InScene, .02f);
		HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Eye, Before.Eye)) == 0);
		Controller.Input(InScene, {&Press, 1}, false, false);
		Controller.Advance(InScene, .02f);
		const auto Moved = ReadCameraPose(InScene, InHandle);
		HYP_CHECK(std::abs(Length(Subtract(Moved.Eye, Before.Eye)) - Distance) < .0001f);
		HYP_CHECK(Length(Subtract(Moved.Forward, Before.Forward)) < .0001f);
		HYP_CHECK(*InScene.FindNode(InHandle)->Camera() == InCamera);
		Controller.Input(InScene, {&Release, 1}, false, false);
		Controller.Advance(InScene, .02f);
		HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Eye, Moved.Eye)) == 0);
		// A physically held key can resume when RMB is pressed again.
		Controller.Input(InScene, {&Press, 1}, false, false);
		Controller.Advance(InScene, .02f);
		const auto Resumed = ReadCameraPose(InScene, InHandle);
		HYP_CHECK(std::abs(Length(Subtract(Resumed.Eye, Moved.Eye)) - Distance) < .0001f);
		const auto KeyUp = CameraKey(Key, false);
		Controller.Input(InScene, {&KeyUp, 1}, false, false);
		Controller.Advance(InScene, .02f);
		HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Eye, Resumed.Eye)) == 0);
	}
	// Releasing RMB in the same event batch as a key press must still stop movement.
	const std::array Events{CameraKey(EKey::W), Release};
	const auto Before = ReadCameraPose(InScene, InHandle);
	Controller.Input(InScene, Events, false, false);
	Controller.Advance(InScene, .02f);
	HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Eye, Before.Eye)) == 0);
}

void CheckFlyLook(FSceneInstance& InScene, FSceneHandle InHandle, const FSceneCamera& InCamera)
{
	FSceneCameraController Controller(ESceneCameraNavigationMode::Fly);
	InScene.SetCameraView(InHandle, SceneCameraTransform({2, 3, 9}, {2, 3, 8}), InCamera);
	const auto Before = ReadCameraPose(InScene, InHandle);
	const auto Press = CameraButton();
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = 220;
	Move.Y = 110;
	Controller.Input(InScene, {&Move, 1}, false, false);
	HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Forward, Before.Forward)) == 0);
	Controller.Input(InScene, {&Press, 1}, false, false);
	Controller.Input(InScene, {&Move, 1}, false, false);
	const auto Looked = ReadCameraPose(InScene, InHandle);
	HYP_CHECK(Looked.Forward.X > 0 && Looked.Forward.Y < 0);
	HYP_CHECK(Length(Subtract(Looked.Eye, Before.Eye)) == 0);
	HYP_CHECK(*InScene.FindNode(InHandle)->Camera() == InCamera);
	const auto Key = CameraKey(EKey::W);
	Controller.Input(InScene, {&Key, 1}, false, false);
	Controller.Advance(InScene, .02f);
	const auto Moved = ReadCameraPose(InScene, InHandle);
	const auto Direction = Normalize(Subtract(Moved.Eye, Looked.Eye));
	HYP_CHECK(Dot(Direction, Looked.Forward) > .9999f);
	for (unsigned Index = 0; Index < 100; ++Index)
	{
		Move.X += 15;
		Move.Y += 500;
		Controller.Input(InScene, {&Move, 1}, false, false);
	}
	const auto Clamped = ReadCameraPose(InScene, InHandle);
	HYP_CHECK(Length(Subtract(Clamped.Eye, Moved.Eye)) == 0);
	HYP_CHECK(IsFinite(Clamped.Forward) && std::abs(Clamped.Forward.Y) < 1);
	HYP_CHECK(std::abs(Dot(Clamped.Forward, Clamped.Up)) < .0001f);
	HYP_CHECK(*InScene.FindNode(InHandle)->Camera() == InCamera);
	const auto Release = CameraButton(false);
	Controller.Input(InScene, {&Release, 1}, false, false);
	Move.X += 20;
	Controller.Input(InScene, {&Move, 1}, false, false);
	Controller.Advance(InScene, .02f);
	const auto Stopped = ReadCameraPose(InScene, InHandle);
	HYP_CHECK(Length(Subtract(Stopped.Eye, Clamped.Eye)) == 0);
	HYP_CHECK(Length(Subtract(Stopped.Forward, Clamped.Forward)) == 0);
}

void CheckFlyInterruptions(FSceneInstance& InScene, FSceneHandle InHandle)
{
	FSceneCameraController Controller(ESceneCameraNavigationMode::Fly);
	const std::array Press{CameraButton(), CameraKey(EKey::W)};
	for (unsigned Mode = 0; Mode < 4; ++Mode)
	{
		Controller.Input(InScene, Press, false, false);
		Controller.Advance(InScene, .02f);
		const auto Before = ReadCameraPose(InScene, InHandle);
		if (Mode < 2)
		{
			Controller.Input(InScene, {}, Mode == 0, Mode == 1);
		}
		else if (Mode == 2)
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Controller.Input(InScene, {&Focus, 1}, true, true);
			Focus.bDown = true;
			Controller.Input(InScene, {&Focus, 1}, true, true);
		}
		else
		{
			Controller.Reset();
		}
		const auto Repeat = CameraKey(EKey::W, true, true);
		Controller.Input(InScene, {&Repeat, 1}, false, false);
		Controller.Advance(InScene, .02f);
		HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Eye, Before.Eye)) == 0);
		Controller.Input(InScene, Press, false, false);
		Controller.Advance(InScene, .02f);
		HYP_CHECK(Length(Subtract(ReadCameraPose(InScene, InHandle).Eye, Before.Eye)) > .001f);
		Controller.Reset();
	}
}

void CheckFlyCamera(FSceneFixture& InFixture)
{
	auto& Scene = *InFixture.Scene;
	const auto Handle = *GetSceneNavigationCamera(Scene);
	const auto Camera = *Scene.FindNode(Handle)->Camera();
	FSceneNodeView View;
	HYP_CHECK(Scene.GetNodeView(Handle, View));
	const auto Original = View.World;
	CheckFlyTranslation(Scene, Handle, Camera);
	CheckFlyLook(Scene, Handle, Camera);
	CheckFlyInterruptions(Scene, Handle);
	CheckFlyWheel(Scene, Handle, Camera);
	CheckFlySpeedInterruptions(Scene, Handle);
	Scene.SetCameraView(Handle, Original, Camera);
	std::cout
	    << "Fly camera: RMB gating, fixed-eye look, wheel speed/dolly, speed limits and interruption recovery passed\n";
}

void CheckSuspendedNavigation()
{
	FSceneCameraController Controller(ESceneCameraNavigationMode::Fly);
	FInputEvent Focus;
	Focus.Type = EEventType::Focus;
	Controller.SuspendInput({&Focus, 1});
	Focus.bDown = true;
	Controller.SuspendInput({&Focus, 1});
	FSceneCameraView Camera;
	Camera.Lens.FocusDistance = 120;
	Controller.Input(Camera, {}, false, false);
	HYP_CHECK(Controller.GetMovementSpeed(Camera) == 120);
	Controller.SuspendInput({});
	Camera.Lens.FocusDistance = 240;
	Controller.Input(Camera, {}, false, false);
	HYP_CHECK(Controller.GetMovementSpeed(Camera) == 120);
}

void CheckStructuralResources(FSceneFixture& InFixture)
{
	auto& Scene = *InFixture.Scene;
	auto& Document = InFixture.Document;
	const auto Model = Scene.GetNodes(ESceneNodeKind::Model).front();
	Document.ReplaceSelection(FSceneSelection(Model));
	const auto Data = Scene.FindNode(Model)->Model()->Data;
	const auto Duplicate = Document.CommitDuplicate(Model);
	HYP_CHECK(Scene.FindNode(Duplicate)->Model()->Data == Data);
	Document.Undo();
	HYP_CHECK(Document.Selection().Primary() == Model && !Scene.FindNode(Duplicate));
	Document.Redo();
	const auto Restored = *Document.Selection().Primary();
	HYP_CHECK(Restored != Duplicate && Scene.FindNode(Restored)->Model()->Data == Data);
	std::string Clipboard;
	Document.SetClipboardProvider({[&]
	                               {
		                               return Clipboard;
	                               },
	                               [&](const std::string& InToken, const std::string&)
	                               {
		                               Clipboard = InToken;
	                               }});
	Document.CopySelection(Document.Id(), Scene.GetRevision());
	Document.PasteClipboard(Document.Id(), Scene.GetRevision());
	const auto Pasted = *Document.Selection().Primary();
	HYP_CHECK(Scene.FindNode(Pasted)->Model()->Data == Data);
	const auto CopiedModel = *Scene.FindNode(Pasted)->Model();
	Document.Undo();
	Clipboard.clear();
	Document.Redo();
	HYP_CHECK(*Scene.FindNode(*Document.Selection())->Model() == CopiedModel);
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.VisibleItems > 0);
	const auto Output = std::filesystem::absolute(PathFromUtf8("scene-save/测试目录/编辑场景.hasset"));
	Document.Save(PathToUtf8(Output)).Get(InFixture.Tasks);
	Document.PollSave();
	HYP_CHECK(!Document.IsDirty());
	const auto Saved = InFixture.Assets.LoadAsync<FSceneManifest>(Output).Get(InFixture.Tasks);
	HYP_CHECK(SceneModelCount(*Saved) == 3);
	const auto Snapshot = Serialize(Scene.Snapshot(Output));
	Scene.Load(Output);
	Document.Reset();
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	do
	{
		InFixture.Tick();
	} while (!Scene.GetStatus().bReady && std::chrono::steady_clock::now() < Deadline);
	HYP_CHECK(Scene.GetStatus().bReady && Serialize(Scene.Snapshot(Output)) == Snapshot);
	HYP_CHECK(!Document.ClipboardInfo().bCanPaste);
	Document.SetClipboardProvider({});
}

void CheckSnapshotIsolationAndFailure(FSceneFixture& InFixture)
{
	auto& F = InFixture;
	auto& Scene = *F.Scene;
	FSceneNode Rig;
	Rig.Id = "save-rig";
	Rig.Local() = Translation({1, 2, 0});
	const auto Parent = Scene.AddNode(Rig);
	const auto Model = Scene.GetNodes(ESceneNodeKind::Model).front();
	Scene.Reparent(Model, Parent, ESceneReparentMode::KeepWorld);
	auto CameraNode = MakeSceneCameraNode("save-camera", {2, 1, 9}, {});
	CameraNode.Parent() = "save-rig";
	const auto Camera = Scene.AddNode(CameraNode);
	const auto Light = Scene.AddNode(MakeSceneDirectionalLightNode("save-light"));
	auto Settings = Scene.GetSettings();
	Settings.DefaultCamera = Camera;
	Scene.SetSettings(Settings);
	Scene.Tick();
	HYP_CHECK(Scene.GetNodes(ESceneNodeKind::Camera).size() == 2);
	HYP_CHECK(Scene.GetNodes(ESceneNodeKind::DirectionalLight).size() == 2);
	const auto Path = std::filesystem::absolute("scene-save/nodes/SnapshotA.hasset");
	const auto Expected = Serialize(Scene.Snapshot(Path));
	const auto Save = F.Document.Save(PathToUtf8(Path));
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
	const auto Failed = F.Document.Save("scene-save/InjectedFailure.hasset");
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
	const auto Outcome = F.Document.TakeSaveOutcome();
	HYP_CHECK(bRejected && Outcome && !Outcome->bSucceeded &&
	          Outcome->Error.find("Injected scene save IO failure") != std::string::npos);
	HYP_CHECK(Serialize(Scene.Snapshot(Path)) == Live);
	std::cout << "Mixed hierarchy SaveAsync freezes camera/light A; live B and IO failure status remain intact\n";
}

void CheckEmptyAndNodeOnlySave(FSceneFixture& InFixture)
{
	auto& F = InFixture;
	auto& Scene = *F.Scene;
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
			Group.Local() = Translation({2, 0, 0});
			Scene.AddNode(Group);
		}
		if (Stage == 2)
		{
			auto Camera = MakeSceneCameraNode("saved-camera", {0, 1, 6}, {});
			Camera.Parent() = "saved-group";
			FSceneSettings Settings;
			Settings.DefaultCamera = Scene.AddNode(Camera);
			Scene.AddNode(MakeSceneDirectionalLightNode("saved-sun"));
			Scene.AddNode(MakeSceneEnvironmentLightNode("saved-environment"));
			Scene.SetSettings(Settings);
		}
		F.Tick();
		const auto Path = std::filesystem::absolute("scene-save/NodeOnly" + std::to_string(Stage) + ".hasset");
		const auto Snapshot = Scene.Snapshot(Path);
		HYP_CHECK(Snapshot.Assets.empty() && SceneModelCount(Snapshot) == 0);
		F.Document.Save(PathToUtf8(Path)).Get(F.Tasks);
		const auto Saved = F.Assets.LoadAsync<FSceneManifest>(Path).Get(F.Tasks);
		HYP_CHECK(Serialize(*Saved) == Serialize(Snapshot));
		Scene.Load(Path);
		F.Document.Reset();
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
		FSceneFixture Fixture;
		CheckContinuousCamera(Fixture);
		CheckCameraInterruptions(Fixture);
		CheckFlyCamera(Fixture);
		CheckReusableMouse(Fixture);
		CheckSuspendedNavigation();
		CheckStructuralResources(Fixture);
		CheckSnapshotIsolationAndFailure(Fixture);
		CheckEmptyAndNodeOnlySave(Fixture);
		std::cout << "Shared navigation, resource-preserving history and persistence passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
