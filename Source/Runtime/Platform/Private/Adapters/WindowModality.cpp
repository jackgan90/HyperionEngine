#include "WindowModality.h"
#include <map>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Hyperion
{
#ifdef _WIN32
namespace
{
// These windows have already regained their native owner and enabled state.
// Only the show deferred until their owner restores remains Platform-owned.
struct FRetiredWindowVisibility
{
	HWND Main{};
};

thread_local std::map<HWND, FRetiredWindowVisibility> RetiredVisibility;

void CheckNative(bool bInSuccess)
{
	if (!bInSuccess)
	{
		throw std::runtime_error("Window group native operation failed: " + std::to_string(GetLastError()));
	}
}

void SetNativeOwner(HWND InWindow, HWND InOwner)
{
	SetLastError(0);
	const auto Previous = SetWindowLongPtrW(InWindow, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(InOwner));
	CheckNative(Previous != 0 || GetLastError() == 0);
}

void SetEnabled(HWND InWindow, bool bInEnabled)
{
	EnableWindow(InWindow, bInEnabled);
	CheckNative(IsWindow(InWindow) && (IsWindowEnabled(InWindow) != FALSE) == bInEnabled);
}

void SetVisible(HWND InWindow, bool bInVisible)
{
	WINDOWPLACEMENT Placement{sizeof(WINDOWPLACEMENT)};
	CheckNative(GetWindowPlacement(InWindow, &Placement) != FALSE);
	const bool bWasMinimized = IsIconic(InWindow) != FALSE;
	CheckNative(SetWindowPos(InWindow, nullptr, 0, 0, 0, 0,
	                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
	                             (bInVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)) != FALSE);
	// SDL's SHOWN handler applies pending flags and can normalize a hidden
	// minimized HWND. Restore its placement without activation after that handler.
	if (bInVisible && bWasMinimized && !IsIconic(InWindow))
	{
		Placement.showCmd = SW_SHOWMINNOACTIVE;
		CheckNative(SetWindowPlacement(InWindow, &Placement) != FALSE);
	}
}

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

void PlaceBehind(HWND InWindow, HWND InAbove)
{
	if (!IsAbove(InAbove, InWindow))
	{
		CheckNative(SetWindowPos(InWindow, InAbove, 0, 0, 0, 0,
		                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER) != FALSE);
	}
}
} // namespace

void SynchronizeRetiredWindowVisibility()
{
	for (auto It = RetiredVisibility.begin(); It != RetiredVisibility.end();)
	{
		if (!IsWindow(It->first) || (It->second.Main && !IsWindow(It->second.Main)))
		{
			It = RetiredVisibility.erase(It);
		}
		else if (!It->second.Main || !IsIconic(It->second.Main))
		{
			SetVisible(It->first, true);
			if (It->second.Main)
			{
				PlaceBehind(It->second.Main, It->first);
			}
			It = RetiredVisibility.erase(It);
		}
		else
		{
			++It;
		}
	}
}

void ForgetRetiredWindowVisibility(FNativeSurface InSurface) noexcept
{
	std::erase_if(RetiredVisibility,
	              [&](const auto& InEntry)
	              {
		              return InEntry.first == InSurface.Handle || InEntry.second.Main == InSurface.Handle;
	              });
}

void ReparentRetiredWindowVisibility(FNativeSurface InWindow, FNativeSurface InOwner)
{
	const auto It = RetiredVisibility.find(static_cast<HWND>(InWindow.Handle));
	if (It != RetiredVisibility.end())
	{
		It->second.Main = static_cast<HWND>(InOwner.Handle);
	}
}

void FNativeWindowModality::ValidateMain(FNativeSurface InMain)
{
	const auto Handle = static_cast<HWND>(InMain.Handle);
	if (!IsWindow(Handle) || GetWindowThreadProcessId(Handle, nullptr) != GetCurrentThreadId() ||
	    GetWindow(Handle, GW_OWNER))
	{
		throw std::invalid_argument("Window group requires an unowned Main window on the current thread");
	}
}

FNativeWindowModality::FNativeWindowModality(FNativeSurface InMain, FNativeSurface InWindow)
    : Main(InMain), Window(InWindow)
{
	const auto Handle = static_cast<HWND>(Window.Handle);
	if (!IsWindow(Handle) || GetWindowThreadProcessId(Handle, nullptr) != GetCurrentThreadId() ||
	    GetWindow(Handle, GW_OWNER) != Main.Handle || (GetWindowLongPtrW(Handle, GWL_EXSTYLE) & WS_EX_TOPMOST))
	{
		throw std::invalid_argument("Window group member must be directly owned by Main and not topmost");
	}
	bVisible = IsWindowVisible(Handle) != FALSE;
}

void FNativeWindowModality::Block()
{
	if (bBlocked)
	{
		return;
	}
	const auto Handle = static_cast<HWND>(Window.Handle);
	OriginalOwner = GetWindow(Handle, GW_OWNER);
	bEnabled = IsWindowEnabled(Handle) != FALSE;
	if (RetiredVisibility.erase(Handle))
	{
		// Re-registration takes over an earlier cleanup's deferred visibility.
		bVisible = true;
	}
	else if (!IsMainMinimized(Main))
	{
		bVisible = IsWindowVisible(Handle) != FALSE;
	}
	bHiddenByMain = bVisible && !IsWindowVisible(Handle);
	bBlocked = true;
	try
	{
		SetEnabled(Handle, false);
		SetNativeOwner(Handle, nullptr);
		Synchronize();
	}
	catch (...)
	{
		Restore();
		throw;
	}
}

bool FNativeWindowModality::IsMainMinimized(FNativeSurface InMain)
{
	return IsIconic(static_cast<HWND>(InMain.Handle)) != FALSE;
}

void FNativeWindowModality::Synchronize()
{
	if (!bBlocked)
	{
		if (!IsMainMinimized(Main))
		{
			bVisible = IsWindowVisible(static_cast<HWND>(Window.Handle)) != FALSE;
		}
		return;
	}
	const auto Handle = static_cast<HWND>(Window.Handle);
	const auto MainHandle = static_cast<HWND>(Main.Handle);
	CheckNative(IsWindow(Handle) && IsWindow(MainHandle));
	if (IsMainMinimized(Main))
	{
		if (IsWindowVisible(Handle))
		{
			SetVisible(Handle, false);
			bHiddenByMain = true;
		}
	}
	else
	{
		if (bHiddenByMain && bVisible)
		{
			SetVisible(Handle, true);
			bHiddenByMain = false;
		}
		PlaceBehind(Handle, MainHandle);
	}
}

void FNativeWindowModality::Restore()
{
	if (!bBlocked)
	{
		return;
	}
	const auto Handle = static_cast<HWND>(Window.Handle);
	SetNativeOwner(Handle, static_cast<HWND>(OriginalOwner));
	SetEnabled(Handle, bEnabled);
	if (bHiddenByMain && bVisible)
	{
		if (IsMainMinimized(Main))
		{
			RetiredVisibility.insert_or_assign(Handle, FRetiredWindowVisibility{static_cast<HWND>(Main.Handle)});
		}
		else
		{
			SetVisible(Handle, true);
		}
	}
	if (!IsMainMinimized(Main))
	{
		PlaceBehind(static_cast<HWND>(Main.Handle), Handle);
	}
	bHiddenByMain = false;
	bBlocked = false;
}
#else
void SynchronizeRetiredWindowVisibility()
{
}

void ForgetRetiredWindowVisibility(FNativeSurface) noexcept
{
}

void ReparentRetiredWindowVisibility(FNativeSurface, FNativeSurface)
{
}

void FNativeWindowModality::ValidateMain(FNativeSurface)
{
	throw std::runtime_error("Window group modality is unavailable on this platform");
}

FNativeWindowModality::FNativeWindowModality(FNativeSurface InMain, FNativeSurface InWindow)
    : Main(InMain), Window(InWindow)
{
	ValidateMain(Main);
}

void FNativeWindowModality::Block()
{
	ValidateMain(Main);
}

void FNativeWindowModality::Synchronize()
{
	ValidateMain(Main);
}

void FNativeWindowModality::Restore()
{
	ValidateMain(Main);
}

bool FNativeWindowModality::IsMainMinimized(FNativeSurface)
{
	return false;
}
#endif

bool FNativeWindowModality::IsBlocked() const
{
	return bBlocked;
}
} // namespace Hyperion
