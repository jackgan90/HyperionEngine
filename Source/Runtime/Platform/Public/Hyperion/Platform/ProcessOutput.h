#pragma once
#include <functional>
#include <memory>
#include <string_view>

namespace Hyperion
{
// Entrypoint-owned infrastructure. Stop after all producers, before destroying the callback/logging.
class FProcessOutput
{
public:
	using FReceive = std::function<void(bool, std::string_view)>;
	explicit FProcessOutput(FReceive InReceive);
	~FProcessOutput();
	void WriteOriginal(std::string_view InText, bool bInError) noexcept;
	void Stop();
	static bool IsRedirected();
	static void ReportFailure(std::string_view InMessage, bool bInInteractive);

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
