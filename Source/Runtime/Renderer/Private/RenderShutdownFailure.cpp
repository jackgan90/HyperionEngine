#include "RenderShutdownFailure.h"
#include "Hyperion/Core/Core.h"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace Hyperion
{
namespace
{
[[noreturn]] void ReportAndExit(std::string_view InOwner, const char* InReason) noexcept
{
	try
	{
		Log(ELogLevel::Error, "Render shutdown failed; owner=" + std::string(InOwner) +
		                          "; stage=destructor-close; reason=" + InReason +
		                          "; outcome=immediate-process-exit; exit-code=" + std::to_string(EXIT_FAILURE));
	}
	catch (...)
	{
		std::fprintf(stderr,
		             "Render shutdown failed; owner=%.*s; stage=destructor-close; reason=%s; "
		             "outcome=immediate-process-exit; exit-code=%d\n",
		             static_cast<int>(InOwner.size()), InOwner.data(), InReason, EXIT_FAILURE);
	}
	std::fflush(nullptr);
	// Completion remains unknown. Exit before member unwinding can release retained native state.
	std::_Exit(EXIT_FAILURE);
}
} // namespace

[[noreturn]] void ExitAfterRenderShutdownFailure(std::string_view InOwner, std::exception_ptr InFailure) noexcept
{
	try
	{
		if (InFailure)
		{
			std::rethrow_exception(InFailure);
		}
	}
	catch (const std::exception& Failure)
	{
		ReportAndExit(InOwner, Failure.what());
	}
	catch (...)
	{
		ReportAndExit(InOwner, "Unknown exception");
	}
	ReportAndExit(InOwner, "Missing failure");
}
} // namespace Hyperion
