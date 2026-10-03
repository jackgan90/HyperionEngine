#pragma once

namespace Hyperion
{
class FWindow;
void CheckWindowGroupOwnership(const FWindow& InWindow);
FWindow* GetWindowGroupInputOwner(const FWindow& InWindow);
void RemoveWindowFromGroup(FWindow& InWindow) noexcept;
} // namespace Hyperion
