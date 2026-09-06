#include "Support/TestSupport.h"
#include <Hyperion/Core/Core.h>
#include <Hyperion/Tasks/TaskSystem.h>
#include <array>
#include <atomic>
#include <iostream>
#include <thread>

int main()
{
	try
	{
		{
			Hyperion::FTaskSystem Tasks(1, 2);
			const auto Main = std::this_thread::get_id();
			std::array<std::thread::id, 4> Ids;
			std::array<Hyperion::FTaskHandle, 4> Routed;
			Routed[0] = Tasks.Dispatch({Hyperion::EDomain::Main},
			                           [&]
			                           {
				                           Ids[0] = std::this_thread::get_id();
			                           });
			Routed[1] = Tasks.Dispatch({Hyperion::EDomain::Render},
			                           [&]
			                           {
				                           Ids[1] = std::this_thread::get_id();
			                           });
			for (unsigned I = 0; I < 2; ++I)
			{
				Routed[I + 2] = Tasks.Dispatch({Hyperion::EDomain::Rhi, I},
				                               [&, I]
				                               {
					                               Ids[I + 2] = std::this_thread::get_id();
				                               });
			}
			Tasks.WaitAll(Routed);
			HYP_CHECK(Ids[0] == Main);
			for (unsigned I = 0; I < 4; ++I)
			{
				for (unsigned J = I + 1; J < 4; ++J)
				{
					HYP_CHECK(Ids[I] != Ids[J]);
				}
			}
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
			bool Caught = false;
			try
			{
				Tasks.Wait(Skipped);
			}
			catch (const std::runtime_error&)
			{
				Caught = true;
			}
			HYP_CHECK(Caught && Value == 42);
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
			auto Final = Tasks.Dispatch({Hyperion::EDomain::Main},
			                            [&]
			                            {
				                            Value = 99;
			                            });
			Tasks.Shutdown();
			HYP_CHECK(Final.Ready() && Value == 99);
			HYP_CHECK(Tasks.Statistics().back().Executed >= 2000);
			bool Rejected = false;
			try
			{
				Tasks.Dispatch({Hyperion::EDomain::Worker},
				               []
				               {
				               });
			}
			catch (const std::logic_error&)
			{
				Rejected = true;
			}
			HYP_CHECK(Rejected);
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
