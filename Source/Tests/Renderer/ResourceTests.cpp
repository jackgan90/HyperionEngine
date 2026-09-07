#include "Hyperion/Renderer/RenderSession.h"
#include "Support/TestSupport.h"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

namespace
{
using namespace Hyperion;

template<class Predicate> void Await(const Predicate& InPredicate)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (!InPredicate() && std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	HYP_CHECK(InPredicate());
}

template<class Interface> class TTrackedResource final : public Interface
{
public:
	TTrackedResource(FTaskSystem& InTasks, std::atomic_int& InAlive) : Tasks(InTasks), Alive(InAlive)
	{
		++Alive;
	}

	~TTrackedResource() override
	{
		if (!Tasks.IsCurrent({EDomain::Rhi, 0}))
		{
			std::terminate();
		}
		--Alive;
	}

	const void* GetDeviceIdentity() const noexcept override
	{
		return &Alive;
	}

private:
	FTaskSystem& Tasks;
	std::atomic_int& Alive;
};

class FTestDevice final : public IRHIDevice
{
public:
	explicit FTestDevice(FTaskSystem& InTasks) : Tasks(InTasks)
	{
	}

	const FRHICapabilities& GetCapabilities() const noexcept override
	{
		return Capabilities;
	}

	FRHIFeatureSupport QueryFeature(ERHIFeature) const override
	{
		return {};
	}

	FBuffer CreateBuffer(std::span<const std::byte>) override
	{
		Tasks.Require({EDomain::Rhi, 0});
		return {std::make_shared<TTrackedResource<IRHIBuffer>>(Tasks, Alive)};
	}

	FTexture CreateTexture(const FImage&) override
	{
		Tasks.Require({EDomain::Rhi, 0});
		return {std::make_shared<TTrackedResource<IRHITexture>>(Tasks, Alive)};
	}

	std::vector<FTexture> CreateTexturesAsync(std::span<const FTextureDesc> InTextures) override
	{
		std::vector<FTexture> Result;
		for (std::size_t Index = 0; Index < InTextures.size(); ++Index)
		{
			Result.push_back(CreateTexture({}));
		}
		return Result;
	}

	bool TexturesReady(std::span<const FTexture>) override
	{
		Tasks.Require({EDomain::Rhi, 0});
		return bUploadComplete;
	}

	FPipeline CreatePipeline(const FPipelineDesc&) override
	{
		Tasks.Require({EDomain::Rhi, 0});
		if (bFailPipeline.exchange(false))
		{
			throw std::runtime_error("Expected pipeline failure");
		}
		return {std::make_shared<TTrackedResource<IRHIPipeline>>(Tasks, Alive)};
	}

	std::unique_ptr<IRHISwapchain> CreateSwapchain(const FRHISwapchainDesc&) override
	{
		return {};
	}

	void WaitIdle() override
	{
		Tasks.Require({EDomain::Rhi, 0});
		++IdleCalls;
		bUploadComplete = true;
		Retained.clear();
	}

	void CollectCompletedResources() override
	{
		Tasks.Require({EDomain::Rhi, 0});
		if (bFailCollection.exchange(false))
		{
			throw std::runtime_error("Expected collection failure");
		}
		if (bFrameComplete)
		{
			Retained.clear();
		}
	}

	FDeviceStats Statistics() const override
	{
		return {};
	}

	FTaskSystem& Tasks;
	FRHICapabilities Capabilities;
	std::atomic_int Alive{};
	std::atomic<bool> bUploadComplete{};
	std::atomic<bool> bFrameComplete{};
	std::atomic<bool> bFailPipeline{};
	std::atomic<bool> bFailCollection{};
	std::atomic_int IdleCalls{};
	std::vector<FColorPass> Retained;
};

FRenderResourceDesc Geometry(bool bInTexture = false)
{
	FRenderResourceDesc Result;
	FRenderGeometryDesc Mesh;
	Mesh.Vertices.resize(3 * sizeof(FVec3));
	Mesh.VertexStride = sizeof(FVec3);
	Mesh.Indices = {0, 1, 2};
	Mesh.Bounds = {{-.5f, -.5f, .25f}, {.5f, .5f, .75f}, true};
	Result.Geometries.push_back(std::move(Mesh));
	Result.Materials.emplace_back();
	Result.Materials[0].Pipeline.bDepthTest = true;
	Result.Sections.push_back({0, 0, 0, 3});
	if (bInTexture)
	{
		Result.Textures.emplace_back();
	}
	return Result;
}

void CheckSharedUpload(FTaskSystem& InTasks, FTestDevice& InDevice, FShaderCompiler& InCompiler)
{
	FRenderSession Session(InTasks, InDevice, InCompiler);
	auto Identity = std::make_shared<const int>(1);
	auto First = Session.GetResources().Request(Identity, 1, "layout;srgb",
	                                            []
	                                            {
		                                            return Geometry(true);
	                                            });
	auto Second = Session.GetResources().Request(Identity, 1, "layout;srgb",
	                                             []() -> FRenderResourceDesc
	                                             {
		                                             throw std::runtime_error("duplicate");
	                                             });
	HYP_CHECK(First == Second);
	FRenderPrimitiveState State;
	State.Resource = First;
	auto A = Session.GetScene().Create(State);
	auto B = Session.GetScene().Create(State);
	State.Resource.reset();
	Await(
	    [&]
	    {
		    return First->GetStatus() == ERenderResourceStatus::Uploading;
	    });
	InTasks.Wait(A.Remove());
	First.reset();
	HYP_CHECK(B.GetStatus().State == ERenderPrimitiveStatus::PendingResources);
	HYP_CHECK(InDevice.Alive == 5);
	InDevice.bUploadComplete = true;
	Await(
	    [&]
	    {
		    return B.GetStatus().State == ERenderPrimitiveStatus::Ready;
	    });
	HYP_CHECK(A.GetStatus().State == ERenderPrimitiveStatus::Removed);
	HYP_CHECK(Session.GetResources().Statistics().GeometryUploads == 1);
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              const auto Snapshot = PrepareSceneSnapshot(Session.GetScene().Collect({}));
		                              InTasks.Wait(InTasks.Dispatch(
		                                  {EDomain::Rhi, 0},
		                                  [&]
		                                  {
			                                  InDevice.Retained = Session.GetResources().BuildPasses(Snapshot);
		                                  }));
	                              }));
	InTasks.Wait(B.Remove());
	Second.reset();
	HYP_CHECK(InDevice.Alive == 5); // Native frame references outlive the scene and all public leases.
	InDevice.bFrameComplete = true;
	Await(
	    [&]
	    {
		    return Session.GetResources().Statistics().LiveResources == 0;
	    });
	HYP_CHECK(InDevice.Alive == 0 && InDevice.IdleCalls == 0);
	Session.Close();
}

void CheckKeysAndFailures(FTaskSystem& InTasks, FTestDevice& InDevice, FShaderCompiler& InCompiler)
{
	FRenderSession Session(InTasks, InDevice, InCompiler);
	FTestDevice OtherDevice(InTasks);
	FRenderSession Other(InTasks, OtherDevice, InCompiler);
	auto Identity = std::make_shared<const int>(2);
	auto Failed = Session.GetResources().Request(Identity, 1, "same",
	                                             []() -> FRenderResourceDesc
	                                             {
		                                             throw std::runtime_error("Expected preparation failure");
	                                             });
	Await(
	    [&]
	    {
		    return Failed->GetStatus() == ERenderResourceStatus::Failed;
	    });
	HYP_CHECK(Failed->GetError() == "Expected preparation failure");
	auto Retry = Session.GetResources().Request(Identity, 1, "same",
	                                            []
	                                            {
		                                            return Geometry();
	                                            });
	auto Joined = Session.GetResources().Request(Identity, 1, "same",
	                                             []
	                                             {
		                                             return Geometry();
	                                             });
	HYP_CHECK(Retry == Joined && Retry != Failed);
	auto Version = Session.GetResources().Request(Identity, 2, "same",
	                                              []
	                                              {
		                                              return Geometry();
	                                              });
	auto Layout = Session.GetResources().Request(Identity, 1, "other-layout",
	                                             []
	                                             {
		                                             return Geometry();
	                                             });
	auto Linear = Session.GetResources().Request(Identity, 1, "linear-role",
	                                             []
	                                             {
		                                             return Geometry();
	                                             });
	auto Device = Other.GetResources().Request(Identity, 1, "same",
	                                           []
	                                           {
		                                           return Geometry();
	                                           });
	HYP_CHECK(Version != Retry && Layout != Retry && Linear != Retry && Device != Retry);
	Await(
	    [&]
	    {
		    return Retry->GetStatus() == ERenderResourceStatus::Ready;
	    });
	FRenderPrimitiveState State;
	State.Resource = Retry;
	auto Binding = Session.GetScene().Create(State);
	InTasks.Wait(Session.GetScene().Flush());
	std::promise<void> Gate;
	auto Released = Gate.get_future().share();
	auto Delayed = Session.GetResources().Request(Identity, 3, "same",
	                                              [Released]
	                                              {
		                                              Released.wait();
		                                              return Geometry();
	                                              });
	State.Resource = Delayed;
	State.Revision = 2;
	InTasks.Wait(Session.GetScene().Update({{Binding.GetHandle(), State}}));
	State.Resource = Version;
	State.Revision = 3;
	InTasks.Wait(Session.GetScene().Update({{Binding.GetHandle(), State}}));
	Gate.set_value();
	Await(
	    [&]
	    {
		    return Delayed->GetStatus() == ERenderResourceStatus::Ready;
	    });
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              const auto Snapshot = Session.GetScene().Collect({});
		                              HYP_CHECK(Snapshot.Items[0].State.Resource == Version &&
		                                        Snapshot.Items[0].State.Revision == 3);
	                              }));
	Session.Close(); // Live CPU leases become terminal and cannot keep native resources beyond shutdown.
	HYP_CHECK(Binding.GetStatus().State == ERenderPrimitiveStatus::Removed);
	HYP_CHECK(Retry->GetStatus() == ERenderResourceStatus::Retired && InDevice.Alive == 0);
	Binding.Remove();
}

void CheckQueryFailure(FTaskSystem& InTasks, FTestDevice& InDevice, FShaderCompiler& InCompiler)
{
	FRenderSession Session(InTasks, InDevice, InCompiler);
	std::promise<void> Gate;
	auto Released = Gate.get_future().share();
	const auto Blocker = InTasks.Dispatch({EDomain::Rhi, 1},
	                                      [Released]
	                                      {
		                                      Released.wait();
	                                      });

	struct FGateGuard
	{
		std::promise<void>& Signal;
		bool bReleased{};

		~FGateGuard()
		{
			if (!bReleased)
			{
				Signal.set_value();
			}
		}
	} Guard{Gate};

	auto Pending = Session.GetResources().Request(
	    std::make_shared<const int>(11), 1, "query-failure",
	    [&InTasks, Blocker]
	    {
		    InTasks.Wait(Blocker); // Resumable wait keeps the single Worker available to the control pump.
		    return Geometry();
	    });
	InDevice.bFailCollection = true;
	Await(
	    [&]
	    {
		    return Pending->GetStatus() == ERenderResourceStatus::Failed;
	    });
	HYP_CHECK(Pending->GetError() == "Expected collection failure");
	Pending.reset();
	Gate.set_value();
	Guard.bReleased = true;
	Await(
	    [&]
	    {
		    return Session.GetResources().Statistics().LiveResources == 0;
	    });
	auto Unknown = Session.GetResources().Request(std::make_shared<const int>(12), 1, "non-standard-failure",
	                                              []() -> FRenderResourceDesc
	                                              {
		                                              throw 7;
	                                              });
	Await(
	    [&]
	    {
		    return Unknown->GetStatus() == ERenderResourceStatus::Failed;
	    });
	Unknown.reset();
	Await(
	    [&]
	    {
		    return Session.GetResources().Statistics().LiveResources == 0;
	    });
	HYP_CHECK(InDevice.Alive == 0);
}

void CheckNoFrameCleanup(FTaskSystem& InTasks, FTestDevice& InDevice, FShaderCompiler& InCompiler)
{
	FRenderSession Session(InTasks, InDevice, InCompiler);
	const auto IdleBefore = InDevice.IdleCalls.load();
	InDevice.bFailPipeline = true;
	auto Failed = Session.GetResources().Request(std::make_shared<const int>(7), 1, "pipeline-failure",
	                                             []
	                                             {
		                                             return Geometry();
	                                             });
	FRenderPrimitiveState State;
	State.Resource = Failed;
	auto Binding = Session.GetScene().Create(std::move(State));
	Await(
	    [&]
	    {
		    return Binding.GetStatus().State == ERenderPrimitiveStatus::Failed;
	    });
	HYP_CHECK(Binding.GetStatus().Error == "Expected pipeline failure");
	InTasks.Wait(Binding.Remove());
	Failed.reset();
	Await(
	    [&]
	    {
		    return Session.GetResources().Statistics().LiveResources == 0;
	    });
	HYP_CHECK(InDevice.Alive == 0 && InDevice.IdleCalls == IdleBefore);
	std::promise<void> Gate;
	auto Released = Gate.get_future().share();
	auto Pending = Session.GetResources().Request(std::make_shared<const int>(8), 1, "pending",
	                                              [Released]
	                                              {
		                                              Released.wait();
		                                              return Geometry(true);
	                                              });
	State = {};
	State.Resource = Pending;
	Binding = Session.GetScene().Create(std::move(State));
	InTasks.Wait(Binding.Remove());
	Pending.reset();
	Gate.set_value();
	Await(
	    [&]
	    {
		    return Session.GetResources().Statistics().LiveResources == 0;
	    });
	HYP_CHECK(InDevice.Alive == 0 && InDevice.IdleCalls == IdleBefore);
	// Shutdown joins a still-running producer, independently of frames and Main callbacks.
	std::promise<void> CloseGate;
	auto CloseReleased = CloseGate.get_future().share();
	auto Survivor = Session.GetResources().Request(std::make_shared<const int>(9), 1, "close",
	                                               [CloseReleased]
	                                               {
		                                               CloseReleased.wait();
		                                               return Geometry();
	                                               });
	State = {};
	State.Resource = Survivor;
	Binding = Session.GetScene().Create(std::move(State));
	std::jthread Release(
	    [&CloseGate]
	    {
		    std::this_thread::sleep_for(std::chrono::milliseconds(25));
		    CloseGate.set_value();
	    });
	Session.Close();
	HYP_CHECK(Survivor->GetStatus() == ERenderResourceStatus::Retired);
	HYP_CHECK(Binding.GetStatus().State == ERenderPrimitiveStatus::Removed && InDevice.Alive == 0);
}

void CheckSectionTransactions(FTaskSystem& InTasks, FTestDevice& InDevice, FShaderCompiler& InCompiler)
{
	FRenderSession Session(InTasks, InDevice, InCompiler);
	auto Resource = Session.GetResources().Request(std::make_shared<const int>(10), 1, "section-validation",
	                                               []
	                                               {
		                                               return Geometry();
	                                               });
	Await(
	    [&]
	    {
		    return Resource->GetStatus() == ERenderResourceStatus::Ready;
	    });
	FRenderPrimitiveState Initial;
	Initial.Resource = Resource;
	auto Bindings = Session.GetScene().CreateBatch({Initial, Initial});
	InTasks.Wait(Session.GetScene().Flush());
	auto Valid = Initial;
	Valid.Revision = 2;
	Valid.World = Translation({.25f, 0, 0});
	auto Invalid = Valid;
	Invalid.Section = 1;
	bool bRejected = false;
	try
	{
		InTasks.Wait(Session.GetScene().Update({{Bindings[0].GetHandle(), Valid}, {Bindings[1].GetHandle(), Invalid}}));
	}
	catch (const std::invalid_argument& Error)
	{
		bRejected = std::string(Error.what()) == "Invalid primitive section";
	}
	HYP_CHECK(bRejected);
	HYP_CHECK(Bindings[0].GetStatus().Revision == 1 && Bindings[1].GetStatus().Revision == 1);
	auto Failed = Session.GetScene().Create(Invalid);
	InTasks.Wait(Session.GetScene().Flush());
	HYP_CHECK(Failed.GetStatus().State == ERenderPrimitiveStatus::Failed);
	HYP_CHECK(Failed.GetStatus().Error == "Invalid primitive section");
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              const auto Snapshot = PrepareSceneSnapshot(Session.GetScene().Collect({}));
		                              HYP_CHECK(Snapshot.Items.size() == 2);
		                              for (const auto& Item : Snapshot.Items)
		                              {
			                              HYP_CHECK(Item.State.Revision == 1 && Item.State.World.Values[12] == 0 &&
			                                        Item.State.Section == 0);
		                              }
	                              }));
	InTasks.Wait(Session.GetScene().Update({{Bindings[0].GetHandle(), Valid}, {Bindings[1].GetHandle(), Valid}}));
	HYP_CHECK(Bindings[0].GetStatus().Revision == 2 && Bindings[1].GetStatus().Revision == 2);
	InTasks.Wait(Failed.Remove());
}

void CheckPendingSection(FTaskSystem& InTasks, FTestDevice& InDevice, FShaderCompiler& InCompiler)
{
	FRenderSession Session(InTasks, InDevice, InCompiler);
	InDevice.bUploadComplete = false;
	auto Resource = Session.GetResources().Request(std::make_shared<const int>(11), 1, "pending-section",
	                                               []
	                                               {
		                                               return Geometry(true);
	                                               });
	Await(
	    [&]
	    {
		    return Resource->GetStatus() == ERenderResourceStatus::Uploading;
	    });
	FRenderPrimitiveState State;
	State.Resource = Resource;
	auto Valid = Session.GetScene().Create(State);
	State.Section = 1;
	auto Invalid = Session.GetScene().Create(State);
	InTasks.Wait(Session.GetScene().Flush());
	HYP_CHECK(Invalid.GetStatus().State == ERenderPrimitiveStatus::PendingResources);
	// Resolve resource-dependent validation without producing any frame or Main callback.
	InDevice.bUploadComplete = true;
	Await(
	    [&]
	    {
		    return Resource->GetStatus() == ERenderResourceStatus::Ready;
	    });
	HYP_CHECK(Valid.GetStatus().State == ERenderPrimitiveStatus::Ready);
	HYP_CHECK(Invalid.GetStatus().State == ERenderPrimitiveStatus::Failed);
	HYP_CHECK(Invalid.GetStatus().Error == "Invalid primitive section");
	InTasks.Wait(InTasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    const auto Snapshot = PrepareSceneSnapshot(Session.GetScene().Collect({}));
		    HYP_CHECK(Snapshot.Items.size() == 1 && Snapshot.Items[0].Primitive == Valid.GetHandle());
		    InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
		                                  [&]
		                                  {
			                                  const auto Passes = Session.GetResources().BuildPasses(Snapshot);
			                                  HYP_CHECK(Passes.size() == 1 && Passes[0].Commands.Draws.size() == 1);
		                                  }));
	    }));
	State.Revision = 2;
	State.Section = 0;
	InTasks.Wait(Session.GetScene().Update({{Invalid.GetHandle(), State}}));
	HYP_CHECK(Invalid.GetStatus().State == ERenderPrimitiveStatus::Ready);
	HYP_CHECK(Invalid.GetStatus().Error.empty());
	InTasks.Wait(Invalid.Remove());
	Session.Close();
	HYP_CHECK(Valid.GetStatus().State == ERenderPrimitiveStatus::Removed);
}

void CheckVisibilityAndAggregation(FTaskSystem& InTasks, FTestDevice& InDevice, FShaderCompiler& InCompiler)
{
	FRenderSession Session(InTasks, InDevice, InCompiler);
	auto Resource = Session.GetResources().Request(std::make_shared<const int>(3), 1, "bounds",
	                                               []
	                                               {
		                                               return Geometry();
	                                               });
	Await(
	    [&]
	    {
		    return Resource->GetStatus() == ERenderResourceStatus::Ready;
	    });
	FRenderPrimitiveState State;
	State.Resource = Resource;
	std::vector<FRenderBinding> Bindings;
	for (int Index = 0; Index < 40; ++Index)
	{
		Bindings.push_back(Session.GetScene().Create(State));
	}
	State.World = Translation({3, 0, 0});
	Bindings.push_back(Session.GetScene().Create(State)); // Fully outside.
	State.World = Multiply(Translation({1, 0, 0}), Scale({-2, .2f, 1}));
	Bindings.push_back(Session.GetScene().Create(State)); // Mirrored, non-uniform and intersecting.
	State.bVisible = false;
	Bindings.push_back(Session.GetScene().Create(State));
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              const auto Raw = Session.GetScene().Collect({});
		                              const auto First = PrepareSceneSnapshot(Raw);
		                              const auto Second = PrepareSceneSnapshot(Raw);
		                              HYP_CHECK(First.Items.size() == 41 && Second.Items.size() == 41);
		                              auto Alternate = Raw;
		                              Alternate.View.ViewProjection = Translation({-3.1f, 0, 0});
		                              HYP_CHECK(PrepareSceneSnapshot(Alternate).Items.size() == 1);
		                              HYP_CHECK(First.Items[0].State.Revision == Second.Items[0].State.Revision);
		                              InTasks.Wait(InTasks.Dispatch(
		                                  {EDomain::Rhi, 0},
		                                  [&]
		                                  {
			                                  const auto Passes = Session.GetResources().BuildPasses(First);
			                                  HYP_CHECK(Passes.size() == 1 && Passes[0].Commands.Draws.size() == 41);
			                                  HYP_CHECK(Passes[0].Commands.bClearDepth);
		                                  }));
	                              }));
}
} // namespace

int main()
{
	try
	{
		FTaskSystem Tasks(1, 2);
		FShaderCompiler Compiler(
		    std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
		    std::filesystem::path(HYP_SOURCE_DIR) /
		        "out/shader-cache"); // Generic descriptions require no shader compilation in these tests.
		FTestDevice Device(Tasks);
		CheckSharedUpload(Tasks, Device, Compiler);
		CheckKeysAndFailures(Tasks, Device, Compiler);
		CheckQueryFailure(Tasks, Device, Compiler);
		CheckNoFrameCleanup(Tasks, Device, Compiler);
		CheckSectionTransactions(Tasks, Device, Compiler);
		CheckPendingSection(Tasks, Device, Compiler);
		CheckVisibilityAndAggregation(Tasks, Device, Compiler);
		HYP_CHECK(Device.Alive == 0);
		std::cout << "Resource sharing, retry, supersession, GPU retention, RHI destruction and culling passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
