#include "Hyperion/Renderer/RenderScene.h"
#include "Support/TestSupport.h"
#include <atomic>
#include <future>
#include <iostream>
#include <limits>

namespace
{
using namespace Hyperion;

class FTestPrimitive final : public IRenderPrimitive
{
public:
	FTestPrimitive(FTaskSystem& InTasks, std::atomic_uint32_t& InDestroyed, std::uint32_t InCount)
	    : IRenderPrimitive(InTasks), Destroyed(InDestroyed), Count(InCount)
	{
	}

	~FTestPrimitive() override
	{
		if (!Tasks.IsCurrent({EDomain::Render}))
		{
			std::terminate();
		}
		++Destroyed;
	}

	void Collect(const FRenderView&, std::vector<FRenderItem>& OutItems) const override
	{
		for (std::uint32_t Index = 0; Index < Count; ++Index)
		{
			OutItems.push_back({GetState(), {}});
		}
	}

private:
	std::atomic_uint32_t& Destroyed;
	std::uint32_t Count;
};

FRenderSceneSnapshot Snapshot(FTaskSystem& InTasks, const FRenderSceneClient& InClient)
{
	FRenderSceneSnapshot Result;
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              Result = InClient.Collect({});
	                              }));
	return Result;
}

void CheckOwnedMessages(FTaskSystem& InTasks)
{
	FRenderSceneClient Client(InTasks);
	std::promise<void> Gate;
	auto Released = Gate.get_future().share();
	const auto Blocker = InTasks.Dispatch({EDomain::Render},
	                                      [Released]
	                                      {
		                                      Released.wait();
	                                      });
	FRenderPrimitiveState State;
	State.World = Translation({1, 2, 3});
	auto Binding = Client.Create(State);
	State.World = Translation({99, 99, 99});
	Gate.set_value();
	InTasks.Wait(Blocker);
	auto First = Snapshot(InTasks, Client);
	HYP_CHECK(First.Items.Size() == 1);
	HYP_CHECK(First.Items[0].State.World.Values[12] == 1);
	State.Revision = 2;
	InTasks.Wait(Client.Update({{Binding.GetHandle(), State}}));
	HYP_CHECK(Snapshot(InTasks, Client).Items[0].State.World.Values[12] == 99);
	HYP_CHECK(First.Items[0].State.World.Values[12] == 1);
	InTasks.Wait(Binding.Remove());
	InTasks.Wait(Binding.Remove());
	HYP_CHECK(Snapshot(InTasks, Client).Items.IsEmpty());
	HYP_CHECK(First.Items[0].State.World.Values[12] == 1);
	HYP_CHECK(Binding.GetStatus().State == ERenderPrimitiveStatus::Removed);
}

void CheckBatchesAndIdentity(FTaskSystem& InTasks)
{
	FRenderSceneClient Client(InTasks);
	FRenderSceneClient Other(InTasks);
	auto A = Client.Create({});
	auto B = Client.Create({});
	FRenderPrimitiveState State;
	State.Revision = 2;
	State.World = Translation({4, 0, 0});
	InTasks.Wait(Client.Update({{A.GetHandle(), State}, {B.GetHandle(), State}}));
	auto Both = Snapshot(InTasks, Client);
	HYP_CHECK(Both.Items.Size() == 2);
	HYP_CHECK(Both.Items[0].State.World.Values[12] == 4 && Both.Items[1].State.World.Values[12] == 4);
	State.Revision = 3;
	auto Invalid = State;
	Invalid.World.Values[0] = std::numeric_limits<float>::quiet_NaN();
	bool bRejected = false;
	try
	{
		InTasks.Wait(Client.Update({{A.GetHandle(), State}, {B.GetHandle(), Invalid}}));
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	Both = Snapshot(InTasks, Client);
	HYP_CHECK(Both.Items[0].State.Revision == 2 && Both.Items[1].State.Revision == 2);
	const auto Stale = A.GetHandle();
	InTasks.Wait(A.Remove());
	auto Replacement = Client.Create({});
	HYP_CHECK(Replacement.GetHandle().Slot == Stale.Slot);
	HYP_CHECK(Replacement.GetHandle().Generation != Stale.Generation);
	InTasks.Wait(Client.Update({{Stale, State}}));
	InTasks.Wait(Other.Update({{Replacement.GetHandle(), State}}));
	Both = Snapshot(InTasks, Client);
	HYP_CHECK(Both.Items[0].State.Revision == 1);
	State.Revision = 1;
	InTasks.Wait(Client.Update({{B.GetHandle(), State}}));
	HYP_CHECK(Snapshot(InTasks, Client).Items[1].State.Revision == 2);
}

void CheckPublishedRemovalGeneration(FTaskSystem& InTasks)
{
	FRenderSceneClient Client(InTasks);
	auto Old = Client.Create({});
	InTasks.Wait(Client.PublishGroups({}, {}, {Old.GetHandle()}).Task);
	auto Replacement = Client.Create({});
	HYP_CHECK(Old.GetHandle().Slot == Replacement.GetHandle().Slot);
	HYP_CHECK(Old.GetHandle().Generation != Replacement.GetHandle().Generation);
	InTasks.Wait(Client.RemoveBatch(std::span(&Old, 1)));
	FRenderPrimitiveState Changed;
	Changed.Revision = 2;
	InTasks.Wait(Client.Update({{Replacement.GetHandle(), Changed}}));
	HYP_CHECK(Replacement.GetStatus().Revision == 2);
	auto Next = Client.Create({});
	HYP_CHECK(Replacement.GetHandle().Slot != Next.GetHandle().Slot);
	const auto Current = Snapshot(InTasks, Client);
	HYP_CHECK(Current.Items.Size() == 2 && Current.Items[0].Primitive != Current.Items[1].Primitive);
}

void CheckFailureAndClose(FTaskSystem& InTasks)
{
	std::atomic_uint32_t Destroyed{};
	FRenderBinding Survivor;
	{
		FRenderSceneClient Client(InTasks);
		Survivor = Client.Create({},
		                         [&Destroyed](FTaskSystem& InSystem)
		                         {
			                         return std::make_unique<FTestPrimitive>(InSystem, Destroyed, 2);
		                         });
		auto Empty = Client.Create({},
		                           [&Destroyed](FTaskSystem& InSystem)
		                           {
			                           return std::make_unique<FTestPrimitive>(InSystem, Destroyed, 0);
		                           });
		auto Failed = Client.Create({},
		                            [](FTaskSystem&) -> std::unique_ptr<IRenderPrimitive>
		                            {
			                            throw std::runtime_error("Expected factory failure");
		                            });
		InTasks.Wait(Client.Flush());
		HYP_CHECK(Failed.GetStatus().Error == "Expected factory failure");
		HYP_CHECK(Snapshot(InTasks, Client).Items.Size() == 2);
		HYP_CHECK(Snapshot(InTasks, Client).Items.Size() == 2);
		InTasks.Wait(Failed.Remove());
		Client.Close(); // No GPU frame was ever produced.
		HYP_CHECK(Destroyed == 2);
		HYP_CHECK(Survivor.GetStatus().State == ERenderPrimitiveStatus::Removed);
	}
	InTasks.Wait(Survivor.Remove());
	HYP_CHECK(Destroyed == 2);
}

void CheckDomains(FTaskSystem& InTasks)
{
	HYP_CHECK(InTasks.IsCurrent({EDomain::Main}));
	bool bRejected = false;
	try
	{
		FStaticMeshRenderPrimitive Invalid(InTasks);
	}
	catch (const std::logic_error&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	FRenderSceneClient Client(InTasks);
	bRejected = false;
	try
	{
		Client.Collect({});
	}
	catch (const std::logic_error&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
} // namespace

int main()
{
	try
	{
		FTaskSystem Tasks(1, 2);
		CheckDomains(Tasks);
		CheckOwnedMessages(Tasks);
		CheckBatchesAndIdentity(Tasks);
		CheckPublishedRemovalGeneration(Tasks);
		CheckFailureAndClose(Tasks);
		std::cout << "Primitive ownership, snapshots, transactions, generations, failure and shutdown passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
