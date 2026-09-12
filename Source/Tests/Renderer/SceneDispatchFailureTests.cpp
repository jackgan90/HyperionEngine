#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneBridge.h"
#include "Support/DispatchAllocationFailure.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <new>

using namespace Hyperion;

namespace
{
void CheckRetry(FRenderSession& InSession, FTaskSystem& InTasks, int InFailureIndex, bool bInFirstPublication)
{
	std::cerr << "Case " << InFailureIndex << " first=" << bInFirstPublication << " start\n";
	FScene Scene;
	const auto Camera = Scene.AddNode(MakeSceneCameraNode("camera", {0, 0, 3}, {}));
	Scene.SetSettings({Camera, {}, {}});
	FSceneRenderBridge Bridge(Scene, InSession, InTasks);
	if (!bInFirstPublication)
	{
		Bridge.Flush();
		InTasks.Wait(Bridge.GetReceipt());
		Scene.SetWorldTransform(Camera, Translation({2, 0, 3}));
	}
	const auto Before = bInFirstPublication ? FScenePublicationToken{} : Bridge.GetToken();
	bool bCaught{};
	bool bInjected{};
	{
		Hyperion::Tests::FDispatchAllocationFailure Failure(InFailureIndex);
		try
		{
			Bridge.Flush();
		}
		catch (const std::bad_alloc&)
		{
			bCaught = true;
		}
		bInjected = Failure.WasInjected();
	}
	HYP_CHECK(bInjected && bCaught);
	if (bInFirstPublication)
	{
		bool bUninitialized{};
		try
		{
			Bridge.GetToken();
		}
		catch (const std::logic_error&)
		{
			bUninitialized = true;
		}
		HYP_CHECK(bUninitialized);
	}
	else
	{
		HYP_CHECK(Bridge.GetToken() == Before);
	}
	std::cerr << "Retrying flush\n";
	Bridge.Flush();
	const auto After = Bridge.GetToken();
	HYP_CHECK(After.PublicationSerial == Before.PublicationSerial + 1);
	std::cerr << "Waiting receipt\n";
	InTasks.Wait(Bridge.GetReceipt());
	std::cerr << "Receipt complete\n";
	Bridge.Flush();
	HYP_CHECK(Bridge.GetToken() == After && Bridge.GetSceneError().empty());
	const auto Seed = InSession.FreezeSceneFrame(After);
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              FSceneViewRequest Request;
		                              Request.Width = 320;
		                              Request.Height = 240;
		                              const auto Frame = InSession.ResolveSceneFrame(*Seed, Request);
		                              HYP_CHECK(Frame.Camera == Camera &&
		                                        Frame.View.Eye.X == (bInFirstPublication ? 0 : 2));
	                              }));
	Bridge.Close();
	std::cout << "Dispatch allocation " << InFailureIndex << " first=" << bInFirstPublication
	          << ": original token retained; exact delta retry and close passed\n";
}
} // namespace

int main()
{
	try
	{
		FTaskSystem Tasks{2, 1};
		FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "dispatch-failure-cache"};
		std::unique_ptr<IRHIDevice> Device;
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
		                          }));
		{
			FRenderSession Session(Tasks, *Device, Compiler);
			for (int Index = 0; Index < 2; ++Index)
			{
				CheckRetry(Session, Tasks, Index, true);
				CheckRetry(Session, Tasks, Index, false);
			}
			Session.Close();
		}
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          HYP_CHECK(Device->Statistics().ValidationErrors == 0);
			                          Device.reset();
		                          }));
		Tasks.Shutdown();
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
