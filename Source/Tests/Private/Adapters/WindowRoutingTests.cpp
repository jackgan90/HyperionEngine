#include "Hyperion/Platform/Window.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <iostream>
#include <windows.h>

using namespace Hyperion;

int main()
{
	try
	{
		auto First = std::make_unique<FWindow>("First", FSize{64, 64}, true);
		FWindow Second("Second", {64, 64}, true);
		First->Poll();
		Second.Poll();
		SendMessageW(static_cast<HWND>(Second.Surface().Handle), WM_CLOSE, 0, 0);
		First->Poll();
		Second.Poll();
		HYP_CHECK(!First->ShouldClose() && Second.ShouldClose());
		HYP_CHECK(std::none_of(First->Events().begin(), First->Events().end(),
		                       [](const auto& InEvent)
		                       {
			                       return InEvent.Type == EEventType::Quit;
		                       }));
		HYP_CHECK(std::count_if(Second.Events().begin(), Second.Events().end(),
		                        [](const auto& InEvent)
		                        {
			                        return InEvent.Type == EEventType::Quit;
		                        }) == 1);
		First.reset();
		Second.CancelClose();
		Second.Poll();
		HYP_CHECK(!Second.ShouldClose());
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
