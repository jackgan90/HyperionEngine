#include "Hyperion/Automation/Channel.h"
#include <algorithm>
#include <cstring>

namespace Hyperion
{
namespace
{
std::size_t FrameSize(const FTransportBytes& InBytes)
{
	std::size_t Size{};
	for (std::size_t Index = 0; Index < 4; ++Index)
	{
		Size = (Size << 8) | std::to_integer<unsigned>(InBytes[Index]);
	}
	return Size;
}
} // namespace

FAutomationChannel::FAutomationChannel(std::unique_ptr<ITransportConnection> InConnection, std::size_t InMaxFrame)
    : Connection(std::move(InConnection)), MaxFrame(InMaxFrame)
{
	if (!Connection || !MaxFrame || MaxFrame > 16 * 1024 * 1024)
	{
		throw std::invalid_argument("Invalid automation channel limits");
	}
}

void FAutomationChannel::ValidatePrefix() const
{
	if (Input.size() >= 4 && (!FrameSize(Input) || FrameSize(Input) > MaxFrame))
	{
		throw FTransportError("protocol_error", "Frame length exceeds channel limits");
	}
}

void FAutomationChannel::Poll(bool bInSend)
{
	Connection->Poll();
	auto Bytes = Connection->Receive();
	if (Bytes.size() > 2 * MaxFrame + 8 - Input.size())
	{
		throw FTransportError("protocol_error", "Buffered input limit exceeded");
	}
	Input.insert(Input.end(), Bytes.begin(), Bytes.end());
	ValidatePrefix();
	if (bInSend && !Output.empty() && Connection->State() == ETransportState::Connected)
	{
		const auto& Front = Output.front();
		const auto Count = std::min({Connection->WriteCapacity(), Front.size() - OutputOffset, std::size_t(65536)});
		if (Count &&
		    Connection->Send(FTransportBytes(Front.begin() + OutputOffset, Front.begin() + OutputOffset + Count)))
		{
			OutputOffset += Count;
			OutputBytes -= Count;
			if (OutputOffset == Front.size())
			{
				Output.pop_front();
				OutputOffset = 0;
			}
		}
	}
}

void FAutomationChannel::Send(std::string_view InMessage)
{
	if (InMessage.empty() || InMessage.size() > MaxFrame)
	{
		throw FTransportError("protocol_error", "Output frame exceeds channel limits");
	}
	if (Output.size() >= 32 || InMessage.size() + 4 > 2 * MaxFrame + 8 - OutputBytes)
	{
		throw FTransportError("backpressure", "Peer output queue is full");
	}
	FTransportBytes Bytes(4 + InMessage.size());
	for (std::size_t Index = 0; Index < 4; ++Index)
	{
		Bytes[Index] = std::byte((InMessage.size() >> ((3 - Index) * 8)) & 255);
	}
	std::memcpy(Bytes.data() + 4, InMessage.data(), InMessage.size());
	OutputBytes += Bytes.size();
	Output.push_back(std::move(Bytes));
}

std::optional<std::string> FAutomationChannel::Receive()
{
	ValidatePrefix();
	if (Input.size() < 4 || Input.size() < FrameSize(Input) + 4)
	{
		return {};
	}
	const auto Size = FrameSize(Input);
	std::string Message(reinterpret_cast<const char*>(Input.data() + 4), Size);
	Input.erase(Input.begin(), Input.begin() + Size + 4);
	return Message;
}

ETransportState FAutomationChannel::State() const noexcept
{
	return Connection->State();
}

const FTransportPeer& FAutomationChannel::Peer() const noexcept
{
	return Connection->Peer();
}

bool FAutomationChannel::IsDrained() const noexcept
{
	return Output.empty() && !Connection->HasPendingWrites();
}

void FAutomationChannel::Close() noexcept
{
	Connection->Close();
	Output.clear();
	OutputBytes = 0;
	OutputOffset = 0;
}
} // namespace Hyperion
