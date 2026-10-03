#include "Hyperion/Platform/Window.h"
#include "Hyperion/Platform/WindowGroup.h"
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

void CheckMainModal(FWindow& InMain, FWindow& InAsset)
{
	const auto Main = static_cast<HWND>(InMain.Surface().Handle);
	const auto Asset = static_cast<HWND>(InAsset.Surface().Handle);
	FWindowGroup Group(InMain);
	auto Registration = Group.Register(InAsset);
	FWindow Extra("Second auxiliary", {260, 180});
	Extra.SetOwner(&InMain);
	auto ExtraRegistration = Group.Register(Extra);
	const auto ExtraHandle = static_cast<HWND>(Extra.Surface().Handle);
	ShowWindow(Main, SW_MAXIMIZE);
	InAsset.Raise();
	for (unsigned Index = 0; Index < 2; ++Index)
	{
		Group.SetModalActive(true);
		InAsset.Raise();
		Extra.Raise();
		AwaitWindow(InMain, InAsset,
		            [&]
		            {
			            Group.Synchronize();
			            return IsAbove(Main, Asset) && IsAbove(Main, ExtraHandle) && IsWindowEnabled(Main) &&
			                   !IsWindowEnabled(Asset) && !IsWindowEnabled(ExtraHandle);
		            });
		HYP_CHECK(IsZoomed(Main) && IsWindowVisible(Asset));
		HYP_CHECK(GetActiveWindow() == Main);
		HYP_CHECK(!GetWindow(Main, GW_OWNER));
		HYP_CHECK(!(GetWindowLongPtrW(Main, GWL_EXSTYLE) & WS_EX_TOPMOST));
		InMain.Minimize();
		AwaitWindow(InMain, InAsset,
		            [&]
		            {
			            Group.Synchronize();
			            return InMain.Minimized() && !IsWindowVisible(Asset) && !IsWindowVisible(ExtraHandle);
		            });
		if (Index == 1)
		{
			Group.SetModalActive(false);
			Group.Synchronize();
			HYP_CHECK(!IsWindowVisible(Asset) && !IsWindowVisible(ExtraHandle));
		}
		InMain.Restore();
		AwaitWindow(InMain, InAsset,
		            [&]
		            {
			            Group.Synchronize();
			            return !InMain.Minimized() && IsWindowVisible(Asset) && IsWindowVisible(ExtraHandle);
		            });
		Group.SetModalActive(false);
		InAsset.Raise();
		AwaitWindow(InMain, InAsset,
		            [&]
		            {
			            return IsWindowEnabled(Asset) && IsAbove(Asset, Main);
		            });
		HYP_CHECK(IsZoomed(Main) && GetWindow(Asset, GW_OWNER) == Main);
		HYP_CHECK(IsWindowEnabled(ExtraHandle) && GetWindow(ExtraHandle, GW_OWNER) == Main);
	}
	InMain.Restore();
}

template<typename TCallable> void RequireRejected(TCallable InCall)
{
	bool bRejected{};
	try
	{
		InCall();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckDynamicMembership()
{
	FWindow Main("Group lifecycle Main", {400, 300});
	FWindow Other("Other group Main", {200, 150});
	FWindowGroup Group(Main);
	FWindowGroup OtherGroup(Other);
	FWindowRegistration Expired;
	Group.SetModalActive(true);
	{
		FWindow First("First auxiliary", {240, 160});
		First.SetOwner(&Main);
		auto Token = Group.Register(First);
		const auto Handle = static_cast<HWND>(First.Surface().Handle);
		HYP_CHECK(!IsWindowEnabled(Handle) && Group.IsInputBlocked(First));
		RequireRejected(
		    [&]
		    {
			    auto Duplicate = Group.Register(First);
		    });
		RequireRejected(
		    [&]
		    {
			    auto CrossGroup = OtherGroup.Register(First);
		    });
		RequireRejected(
		    [&]
		    {
			    First.SetOwner(nullptr);
		    });
		RequireRejected(
		    [&]
		    {
			    auto Invalid = Group.Register(Other);
		    });
		RequireRejected(
		    [&]
		    {
			    FWindowGroup Duplicate(Main);
		    });
		HYP_CHECK(!IsWindowEnabled(Handle));
		{
			FWindow Second("Late auxiliary", {180, 120});
			Second.SetOwner(&Main);
			Expired = Group.Register(Second);
			HYP_CHECK(Expired.IsInputBlocked());
		}
		HYP_CHECK(!Expired.IsInputBlocked() && !IsWindowEnabled(Handle));
		Token.Reset();
		HYP_CHECK(IsWindowEnabled(Handle));
		Token = Group.Register(First);
		HYP_CHECK(Token.IsInputBlocked());
	}
	Group.SetModalActive(false);
	FWindow Auxiliary("Surviving auxiliary", {200, 160});
	Auxiliary.SetOwner(&Other);
	{
		// A separate lifetime tests a token outliving its group.
		FWindow Independent("Independent Main", {200, 160});
		FWindow Child("Independent child", {160, 120});
		Child.SetOwner(&Independent);
		{
			FWindowGroup Temporary(Independent);
			Expired = Temporary.Register(Child);
			Temporary.SetModalActive(true);
		}
		HYP_CHECK(!Expired.IsInputBlocked() && IsWindowEnabled(static_cast<HWND>(Child.Surface().Handle)));
	}
	Expired.Reset();
}

void CheckPreservedState()
{
	FWindow Main("State Main", {400, 300});
	FWindowGroup Group(Main);
	FWindow Hidden("Hidden member", {240, 160}, true);
	FWindow Minimized("Minimized member", {240, 160});
	FWindow Maximized("Maximized member", {240, 160});
	Hidden.SetOwner(&Main);
	Minimized.SetOwner(&Main);
	Maximized.SetOwner(&Main);
	auto HiddenToken = Group.Register(Hidden);
	auto MinimizedToken = Group.Register(Minimized);
	auto MaximizedToken = Group.Register(Maximized);
	const auto HiddenHandle = static_cast<HWND>(Hidden.Surface().Handle);
	const auto MinHandle = static_cast<HWND>(Minimized.Surface().Handle);
	const auto MaxHandle = static_cast<HWND>(Maximized.Surface().Handle);
	EnableWindow(HiddenHandle, FALSE); // Establish captured enabled state before modal entry.
	Minimized.Minimize();
	ShowWindow(MaxHandle, SW_MAXIMIZE);
	AwaitWindow(Main, Minimized,
	            [&]
	            {
		            return Minimized.Minimized();
	            });
	Group.SetModalActive(true);
	Main.Minimize();
	AwaitWindow(Main, Minimized,
	            [&]
	            {
		            Group.Synchronize();
		            return Main.Minimized() && !IsWindowVisible(MaxHandle) && !IsWindowVisible(MinHandle);
	            });
	Main.Restore();
	AwaitWindow(Main, Minimized,
	            [&]
	            {
		            Group.Synchronize();
		            return !Main.Minimized() && IsWindowVisible(MaxHandle) && IsWindowVisible(MinHandle);
	            });
	Group.SetModalActive(false);
	Main.Poll();
	Minimized.Poll();
	HYP_CHECK(!IsWindowVisible(HiddenHandle) && !IsWindowEnabled(HiddenHandle));
	HYP_CHECK(IsIconic(MinHandle) && Minimized.Minimized());
	HYP_CHECK(IsZoomed(MaxHandle) && IsWindowEnabled(MaxHandle));
}

void CheckEntryWhileMinimized()
{
	FWindow Main("Minimized entry Main", {400, 300});
	FWindow Child("Minimized entry child", {240, 160});
	Child.SetOwner(&Main);
	FWindowGroup Group(Main);
	auto Token = Group.Register(Child);
	const auto Handle = static_cast<HWND>(Child.Surface().Handle);
	Main.Minimize();
	AwaitWindow(Main, Child,
	            [&]
	            {
		            return Main.Minimized() && !IsWindowVisible(Handle);
	            });
	Group.SetModalActive(true);
	Group.SetModalActive(false);
	Main.Restore();
	AwaitWindow(Main, Child,
	            [&]
	            {
		            Group.Synchronize();
		            return !Main.Minimized() && IsWindowVisible(Handle) && IsWindowEnabled(Handle);
	            });
}

void CheckFailedNativeEntry()
{
	FWindow Main("Failure Main", {400, 300});
	FWindowGroup Group(Main);
	FWindow First("Failure first", {240, 160});
	FWindow Second("Failure second", {240, 160});
	First.SetOwner(&Main);
	Second.SetOwner(&Main);
	auto FirstToken = Group.Register(First);
	auto SecondToken = Group.Register(Second);
	// Invalidate the member visited last to exercise rollback after one successful
	// native entry. Only the test uses DestroyWindow outside Platform's lifecycle.
	const bool bFirstIsEarlier = std::less<const FWindow*>{}(&First, &Second);
	auto& Surviving = bFirstIsEarlier ? First : Second;
	auto& Broken = bFirstIsEarlier ? Second : First;
	HYP_CHECK(DestroyWindow(static_cast<HWND>(Broken.Surface().Handle)));
	bool bFailed{};
	try
	{
		Group.SetModalActive(true);
	}
	catch (const std::runtime_error&)
	{
		bFailed = true;
	}
	const auto Handle = static_cast<HWND>(Surviving.Surface().Handle);
	HYP_CHECK(bFailed && IsWindowEnabled(Handle) && !Group.IsInputBlocked(Surviving));
	HYP_CHECK(GetWindow(Handle, GW_OWNER) == Main.Surface().Handle);
}

enum class EMinimizedCleanup
{
	Unregister,
	DestroyGroup,
	RegisterAgain,
	DestroyMember
};

void CheckMinimizedCleanup(EMinimizedCleanup InMode)
{
	FWindow Main("Cleanup Main", {400, 300});
	auto Child = std::make_unique<FWindow>("Cleanup child", FSize{240, 160});
	Child->SetOwner(&Main);
	auto Group = std::make_unique<FWindowGroup>(Main);
	auto Token = Group->Register(*Child);
	const auto Handle = static_cast<HWND>(Child->Surface().Handle);
	Group->SetModalActive(true);
	Main.Minimize();
	AwaitWindow(Main, *Child,
	            [&]
	            {
		            Group->Synchronize();
		            return Main.Minimized() && !IsWindowVisible(Handle);
	            });
	if (InMode == EMinimizedCleanup::DestroyGroup)
	{
		Group.reset();
	}
	else
	{
		Token.Reset();
	}
	HYP_CHECK(!IsWindowVisible(Handle) && IsWindowEnabled(Handle));
	if (InMode == EMinimizedCleanup::RegisterAgain)
	{
		Token = Group->Register(*Child);
		HYP_CHECK(Token.IsInputBlocked());
	}
	if (InMode == EMinimizedCleanup::DestroyMember)
	{
		Child.reset();
		Main.Restore();
		Main.Poll();
		HYP_CHECK(!IsWindow(Handle));
		return;
	}
	Main.Restore();
	AwaitWindow(Main, *Child,
	            [&]
	            {
		            if (Group)
		            {
			            Group->Synchronize();
		            }
		            return !Main.Minimized() && IsWindowVisible(Handle);
	            });
	HYP_CHECK((IsWindowEnabled(Handle) != FALSE) == (InMode != EMinimizedCleanup::RegisterAgain));
	HYP_CHECK(Token.IsInputBlocked() == (InMode == EMinimizedCleanup::RegisterAgain));
}

enum class ERetiredOwner
{
	None,
	Visible,
	Minimized
};

void CheckRetiredOwnerTransfer(ERetiredOwner InMode)
{
	FWindow Main("Previous Main", {400, 300});
	FWindow Next("Next Main", {400, 300});
	FWindow Child("Transferred child", {240, 160});
	Child.SetOwner(&Main);
	FWindowGroup Group(Main);
	FWindowGroup NextGroup(Next);
	auto Token = Group.Register(Child);
	const auto Handle = static_cast<HWND>(Child.Surface().Handle);
	Group.SetModalActive(true);
	Main.Minimize();
	AwaitWindow(Main, Child,
	            [&]
	            {
		            Group.Synchronize();
		            return Main.Minimized() && !IsWindowVisible(Handle);
	            });
	Token.Reset();
	if (InMode == ERetiredOwner::Minimized)
	{
		Next.Minimize();
		AwaitWindow(Next, Child,
		            [&]
		            {
			            return Next.Minimized();
		            });
	}
	Child.SetOwner(InMode == ERetiredOwner::None ? nullptr : &Next);
	if (InMode != ERetiredOwner::None)
	{
		Token = NextGroup.Register(Child);
	}
	if (InMode == ERetiredOwner::Minimized)
	{
		Child.Poll();
		HYP_CHECK(!IsWindowVisible(Handle));
		Next.Restore();
	}
	AwaitWindow(Next, Child,
	            [&]
	            {
		            return IsWindowVisible(Handle) && IsWindowEnabled(Handle);
	            });
	HYP_CHECK(Main.Minimized());
	HYP_CHECK(GetWindow(Handle, GW_OWNER) == (InMode == ERetiredOwner::None ? nullptr : Next.Surface().Handle));
}
} // namespace

int main()
{
	try
	{
		CheckDynamicMembership();
		CheckPreservedState();
		CheckEntryWhileMinimized();
		CheckFailedNativeEntry();
		CheckMinimizedCleanup(EMinimizedCleanup::Unregister);
		CheckMinimizedCleanup(EMinimizedCleanup::DestroyGroup);
		CheckMinimizedCleanup(EMinimizedCleanup::RegisterAgain);
		CheckMinimizedCleanup(EMinimizedCleanup::DestroyMember);
		CheckRetiredOwnerTransfer(ERetiredOwner::None);
		CheckRetiredOwnerTransfer(ERetiredOwner::Visible);
		CheckRetiredOwnerTransfer(ERetiredOwner::Minimized);
		FWindow Main("Ownership test main", {480, 320});
		for (unsigned Index = 0; Index < 2; ++Index)
		{
			FWindow Asset("Ownership test asset", {320, 240});
			Asset.SetOwner(&Main);
			CheckOwnershipCycle(Main, Asset);
			CheckActivation(Main, Asset);
			CheckMainModal(Main, Asset);
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
