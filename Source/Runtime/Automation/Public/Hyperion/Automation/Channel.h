#pragma once
#include "Hyperion/Transport/Transport.h"
#include <deque>
#include <optional>
#include <string_view>

namespace Hyperion
{
// Four-byte big-endian length followed by UTF-8 JSON. No platform message boundaries are assumed.
class FAutomationChannel
{
public:
	explicit FAutomationChannel(std::unique_ptr<ITransportConnection> InConnection,
	                            std::size_t InMaxFrame = 4 * 1024 * 1024 + 65536);
	void Poll(bool bInSend = true);
	void Send(std::string_view InMessage);
	std::optional<std::string> Receive();
	ETransportState State() const noexcept;
	const FTransportPeer& Peer() const noexcept;
	bool IsDrained() const noexcept;
	void Close() noexcept;

private:
	void ValidatePrefix() const;
	std::unique_ptr<ITransportConnection> Connection;
	std::size_t MaxFrame{};
	FTransportBytes Input;
	std::deque<FTransportBytes> Output;
	std::size_t OutputOffset{};
	std::size_t OutputBytes{};
};
} // namespace Hyperion
