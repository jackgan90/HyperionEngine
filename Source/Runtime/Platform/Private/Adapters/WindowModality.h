#pragma once
#include <Hyperion/Platform/Window.h>

namespace Hyperion
{
// Visibility restoration can outlive a registration/group, but never a window.
void SynchronizeRetiredWindowVisibility();
void ForgetRetiredWindowVisibility(FNativeSurface InSurface) noexcept;
void ReparentRetiredWindowVisibility(FNativeSurface InWindow, FNativeSurface InOwner);

// Native-only adjustment: never mutates the SDL window hierarchy.
class FNativeWindowModality
{
public:
	FNativeWindowModality(FNativeSurface InMain, FNativeSurface InWindow);
	void Block();
	void Synchronize();
	void Restore();
	bool IsBlocked() const;
	static bool IsMainMinimized(FNativeSurface InMain);
	static void ValidateMain(FNativeSurface InMain);

private:
	FNativeSurface Main;
	FNativeSurface Window;
	void* OriginalOwner{};
	bool bEnabled{};
	bool bVisible{};
	bool bBlocked{};
	bool bHiddenByMain{};
};
} // namespace Hyperion
