#pragma once
#include <exception>
#include <string_view>

namespace Hyperion
{
[[noreturn]] void ExitAfterRenderShutdownFailure(std::string_view InOwner, std::exception_ptr InFailure) noexcept;
} // namespace Hyperion
