#include "Hyperion/Platform/Window.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <windows.h>

using namespace Hyperion;

namespace
{
bool IsAbove(HWND InUpper, HWND InLower)
{
	for (auto Current = GetWindow(InLower, GW_HWNDPREV); Current; Current = GetWindow(Current, GW_HWNDPREV))
	{
		if (Current == InUpper)
		{
			return true;
		}
	}
	return false;
}

void AwaitWindow(FWindow& InMain, FWindow& InAsset, const std::function<bool()>& InReady)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	do
	{
		InMain.Poll();
		InAsset.Poll();
		if (InReady())
		{
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	} while (std::chrono::steady_clock::now() < Deadline);
	throw std::runtime_error("Owned window did not reach its expected native state");
}

void CheckActivation(FWindow& InMain, FWindow& InAsset)
{
	const auto Main = static_cast<HWND>(InMain.Surface().Handle);
	const auto Asset = static_cast<HWND>(InAsset.Surface().Handle);
	HYP_CHECK(GetWindow(Asset, GW_OWNER) == Main);
	HYP_CHECK(!(GetWindowLongPtrW(Asset, GWL_STYLE) & WS_CHILD));
	HYP_CHECK(!(GetWindowLongPtrW(Asset, GWL_EXSTYLE) & WS_EX_TOPMOST));
	for (unsigned Index = 0; Index < 3; ++Index)
	{
		InAsset.Raise();
		InMain.Raise();
		// Activate the owner as a scene click would, without requiring global foreground permission.
		SetActiveWindow(Main);
		AwaitWindow(InMain, InAsset,
		            [&]
		            {
			            return GetActiveWindow() == Main && IsAbove(Asset, Main) && IsWindowVisible(Asset);
		            });
		HYP_CHECK(IsWindowEnabled(Main) && IsWindowEnabled(Asset));
	}
	FWindow Other("Unrelated window", {240, 160});
	const auto Unrelated = static_cast<HWND>(Other.Surface().Handle);
	Other.Raise();
	SetActiveWindow(Unrelated);
	HYP_CHECK(SetWindowPos(Unrelated, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE));
	AwaitWindow(InMain, InAsset,
	            [&]
	            {
		            return IsAbove(Unrelated, Asset) && IsAbove(Asset, Main);
	            });
	InMain.Raise();
	SetActiveWindow(Main);
	AwaitWindow(InMain, InAsset,
	            [&]
	            {
		            return IsAbove(Asset, Main) && IsAbove(Main, Unrelated) && GetActiveWindow() == Main;
	            });
}

void CheckMinimization(FWindow& InMain, FWindow& InAsset)
{
	const auto Main = static_cast<HWND>(InMain.Surface().Handle);
	const auto Asset = static_cast<HWND>(InAsset.Surface().Handle);
	InAsset.Minimize();
	AwaitWindow(InMain, InAsset,
	            [&]
	            {
		            return InAsset.Minimized();
	            });
	HYP_CHECK(!InMain.Minimized() && IsWindowVisible(Main) && IsWindowEnabled(Main));
	InAsset.Restore();
	AwaitWindow(InMain, InAsset,
	            [&]
	            {
		            return !InAsset.Minimized() && IsWindowVisible(Asset);
	            });
	InMain.Minimize();
	AwaitWindow(InMain, InAsset,
	            [&]
	            {
		            return InMain.Minimized() && !IsWindowVisible(Asset);
	            });
	InMain.Restore();
	AwaitWindow(InMain, InAsset,
	            [&]
	            {
		            return !InMain.Minimized() && IsWindowVisible(Asset) && IsAbove(Asset, Main);
	            });
}

void CheckOwnershipCycle(FWindow& InMain, FWindow& InAsset)
{
	bool bRejected = false;
	try
	{
		InMain.SetOwner(&InAsset);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	HYP_CHECK(!GetWindow(static_cast<HWND>(InMain.Surface().Handle), GW_OWNER));
}
} // namespace

int main()
{
	try
	{
		FWindow Main("Ownership test main", {480, 320});
		for (unsigned Index = 0; Index < 2; ++Index)
		{
			FWindow Asset("Ownership test asset", {320, 240});
			Asset.SetOwner(&Main);
			CheckOwnershipCycle(Main, Asset);
			CheckActivation(Main, Asset);
			CheckMinimization(Main, Asset);
			const auto Handle = static_cast<HWND>(Asset.Surface().Handle);
			SendMessageW(Handle, WM_CLOSE, 0, 0);
			Main.Poll();
			Asset.Poll();
			HYP_CHECK(Asset.ShouldClose() && !Main.ShouldClose());
			Asset.CancelClose();
			HYP_CHECK(IsWindow(Handle) && IsWindowVisible(Handle));
			Asset.SetOwner(nullptr);
			HYP_CHECK(!GetWindow(Handle, GW_OWNER));
			Asset.SetOwner(&Main);
		}
		HYP_CHECK(IsWindow(static_cast<HWND>(Main.Surface().Handle)) && !Main.ShouldClose());
		std::cout << "Visible owned-window stacking, focus, minimize/restore, close and recreation passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
