#include "Hyperion/Platform/Window.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <windows.h>

using namespace Hyperion;

namespace
{
struct FDesktopRestore
{
	POINT Cursor{};
	HWND Foreground = GetForegroundWindow();

	FDesktopRestore()
	{
		GetCursorPos(&Cursor);
	}

	~FDesktopRestore()
	{
		SetCursorPos(Cursor.x, Cursor.y);
		if (IsWindow(Foreground))
		{
			SetForegroundWindow(Foreground);
		}
	}
};

struct FWindowInputFixture
{
	FDesktopRestore Restore;
	FWindow Main{"Input test main", {640, 480}};
	FWindow Asset{"Input test asset", {240, 180}};
	unsigned Presses{};
	unsigned Releases{};

	FWindowInputFixture()
	{
		Asset.SetOwner(&Main);
		RECT Work;
		HYP_CHECK(SystemParametersInfoW(SPI_GETWORKAREA, 0, &Work, 0));
		HYP_CHECK(SetWindowPos(Handle(Main), nullptr, Work.left + 20, Work.top + 20, 0, 0,
		                       SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
		HYP_CHECK(SetWindowPos(Handle(Asset), nullptr, Work.left + 360, Work.top + 20, 0, 0,
		                       SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
		Pump();
	}

	static HWND Handle(FWindow& InWindow)
	{
		return static_cast<HWND>(InWindow.Surface().Handle);
	}

	void Pump()
	{
		for (unsigned Index = 0; Index < 3; ++Index)
		{
			Main.Poll();
			Asset.Poll();
			for (const auto& Event : Main.Events())
			{
				if (Event.Type == EEventType::MouseButton && Event.Button == 0)
				{
					Presses += Event.bDown ? 1 : 0;
					Releases += Event.bDown ? 0 : 1;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	void Click(FWindow& InWindow)
	{
		POINT Position{100, 100};
		HYP_CHECK(ClientToScreen(Handle(InWindow), &Position));
		HYP_CHECK(SetCursorPos(Position.x, Position.y));
		HYP_CHECK(WindowFromPoint(Position) == Handle(InWindow));
		// Deliver a real activation click to our own window, including the OS button state.
		INPUT Input{};
		Input.type = INPUT_MOUSE;
		Input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		HYP_CHECK(SendInput(1, &Input, sizeof(INPUT)) == 1);
		Input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		try
		{
			Pump();
		}
		catch (...)
		{
			SendInput(1, &Input, sizeof(INPUT));
			throw;
		}
		HYP_CHECK(SendInput(1, &Input, sizeof(INPUT)) == 1);
		Pump();
	}
};
} // namespace

int main()
{
	try
	{
		FWindowInputFixture Test;
		for (unsigned Index = 0; Index < 2; ++Index)
		{
			Test.Asset.Raise();
			Test.Click(Test.Asset);
			HYP_CHECK(GetForegroundWindow() == FWindowInputFixture::Handle(Test.Asset));
			Test.Presses = Test.Releases = 0;
			Test.Click(Test.Main);
			HYP_CHECK(Test.Presses == 1 && Test.Releases == 1);
			Test.Click(Test.Main);
			std::cout << "Activation double-click delivered " << Test.Presses << " presses and " << Test.Releases
			          << " releases\n";
			HYP_CHECK(Test.Presses == 2 && Test.Releases == 2);
		}
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
