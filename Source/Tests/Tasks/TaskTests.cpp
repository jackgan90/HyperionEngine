#include "Support/TestSupport.h"
#include <Hyperion/Core/Core.h>
#include <Hyperion/Tasks/TaskSystem.h>
#include <array>
#include <atomic>
#include <iostream>
#include <thread>

namespace
{
void CheckThreadRouting(Hyperion::FTaskSystem& InTasks)
{
	const auto Main = std::this_thread::get_id();
	std::array<std::thread::id, 4> Ids;
	std::array<Hyperion::FTaskHandle, 4> Routed;
	Routed[0] = InTasks.Dispatch({Hyperion::EDomain::Main},
	                             [&]
	                             {
		                             Ids[0] = std::this_thread::get_id();
	                             });
	Routed[1] = InTasks.Dispatch({Hyperion::EDomain::Render},
	                             [&]
	                             {
		                             Ids[1] = std::this_thread::get_id();
	                             });
	for (unsigned I = 0; I < 2; ++I)
	{
		Routed[I + 2] = InTasks.Dispatch({Hyperion::EDomain::Rhi, I},
		                                 [&, I]
		                                 {
			                                 Ids[I + 2] = std::this_thread::get_id();
		                                 });
	}
	InTasks.WaitAll(Routed);
	HYP_CHECK(Ids[0] == Main);
	for (unsigned I = 0; I < 4; ++I)
	{
		for (unsigned J = I + 1; J < 4; ++J)
		{
			HYP_CHECK(Ids[I] != Ids[J]);
		}
	}
}

void CheckTaskShutdown(Hyperion::FTaskSystem& InTasks, std::atomic_int& InValue)
{
	auto Final = InTasks.Dispatch({Hyperion::EDomain::Main},
	                              [&]
	                              {
		                              InValue = 99;
	                              });
	InTasks.Shutdown();
	HYP_CHECK(Final.Ready() && InValue == 99);
	HYP_CHECK(InTasks.Statistics().back().Executed >= 2000);
	bool bRejected = false;
	try
	{
		InTasks.Dispatch({Hyperion::EDomain::Worker},
		                 []
		                 {
		                 });
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
		{
			Hyperion::FTaskSystem Tasks(1, 2);
			CheckThreadRouting(Tasks);
			std::atomic_int Value{};
			auto A = Tasks.Dispatch({Hyperion::EDomain::Worker},
			                        [&]
			                        {
				                        Value = 5;
			                        });
			std::array Deps{A};
			auto B = Tasks.Dispatch(
			    {Hyperion::EDomain::Render},
			    [&]
			    {
				    HYP_CHECK(Value == 5);
				    Value = 9;
			    },
			    Deps);
			Tasks.Wait(B);
			HYP_CHECK(Value == 9);
			auto Parent = Tasks.Dispatch({Hyperion::EDomain::Worker},
			                             [&]
			                             {
				                             auto Child = Tasks.Dispatch({Hyperion::EDomain::Worker},
				                                                         [&]
				                                                         {
					                                                         Value = 42;
				                                                         });
				                             Tasks.Wait(Child);
				                             HYP_CHECK(Value == 42);
			                             });
			Tasks.Wait(Parent);
			auto Failed = Tasks.Dispatch({Hyperion::EDomain::Worker},
			                             []
			                             {
				                             throw std::runtime_error("expected failure");
			                             });
			std::array BadDeps{Failed};
			auto Skipped = Tasks.Dispatch(
			    {Hyperion::EDomain::Worker},
			    [&]
			    {
				    Value = -1;
			    },
			    BadDeps);
			bool bCaught = false;
			try
			{
				Tasks.Wait(Skipped);
			}
			catch (const std::runtime_error&)
			{
				bCaught = true;
			}
			HYP_CHECK(bCaught && Value == 42);
			std::vector<Hyperion::FTaskHandle> Work;
			for (int I = 0; I < 2000; ++I)
			{
				Work.push_back(Tasks.Dispatch({Hyperion::EDomain::Worker},
				                              [&]
				                              {
					                              Value.fetch_add(1);
				                              }));
			}
			Tasks.WaitAll(Work);
			HYP_CHECK(Value == 2042);
			CheckTaskShutdown(Tasks, Value);
		}
		HYP_CHECK(Hyperion::MemoryStats(Hyperion::EMemoryTag::Tasks).LiveBytes == 0);
		std::cout << "Task execution integration passed\n";
	}
	catch (const std::exception& E)
	{
		std::cerr << E.what() << '\n';
		return 1;
	}
}
