#include "Hyperion/Tasks/TaskSystem.h"
#include "Support/DispatchAllocationFailure.h"
#include "Support/TestSupport.h"
#include <array>
#include <iostream>

using namespace Hyperion;

int main()
{
	try
	{
		for (int Index = 0; Index < 3; ++Index)
		{
			FTaskSystem Tasks(2, 1);
			std::array<FTaskHandle, 3> Dependencies;
			for (auto& Dependency : Dependencies)
			{
				Dependency = Tasks.Dispatch({EDomain::Main},
				                            []
				                            {
				                            });
			}
			int Ran = 0;
			bool bCaught = false;
			{
				Tests::FDispatchAllocationFailure Failure(Index, "Hyperion::FTaskState::Subscribe");
				try
				{
					Tasks.Dispatch(
					    {EDomain::Main},
					    [&]
					    {
						    ++Ran;
					    },
					    Dependencies);
				}
				catch (const std::bad_alloc&)
				{
					bCaught = true;
				}
				HYP_CHECK(Failure.WasInjected() && bCaught);
			}
			Tasks.PumpMain();
			HYP_CHECK(Ran == 0);
			Tasks.Wait(Tasks.Dispatch(
			    {EDomain::Main},
			    [&]
			    {
				    ++Ran;
			    },
			    Dependencies));
			HYP_CHECK(Ran == 1);
			Tasks.Shutdown();
		}
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
