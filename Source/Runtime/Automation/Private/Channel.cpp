#include "Hyperion/Automation/Channel.h"
#include <algorithm>
#include <cstring>

namespace Hyperion
{
namespace
{
constexpr std::size_t MaxChannelFrameBytes = 16 * 1024 * 1024;
constexpr std::size_t MaxQueuedChannelFrames = 32;
constexpr std::size_t ChannelWriteBytesPerPoll = 65536;
constexpr std::size_t ChannelFramePrefixBytes = 4;
constexpr std::size_t MaxBufferedChannelFrames = 2;

std::size_t FrameSize(const FTransportBytes& InBytes)
{
	std::size_t Size{};
	for (std::size_t Index = 0; Index < ChannelFramePrefixBytes; ++Index)
	{
		Size = (Size << 8) | std::to_integer<unsigned>(InBytes[Index]);
	}
	return Size;
}
} // namespace

FAutomationChannel::FAutomationChannel(std::unique_ptr<ITransportConnection> InConnection, std::size_t InMaxFrame)
    : Connection(std::move(InConnection)), MaxFrame(InMaxFrame)
{
	if (!Connection || !MaxFrame || MaxFrame > MaxChannelFrameBytes)
	{
		throw std::invalid_argument("Invalid automation channel limits");
	}
}

void FAutomationChannel::ValidatePrefix() const
{
	if (Input.size() >= ChannelFramePrefixBytes && (!FrameSize(Input) || FrameSize(Input) > MaxFrame))
	{
		throw FTransportError(TransportErrors::ProtocolError, "Frame length exceeds channel limits");
	}
}

void FAutomationChannel::Poll(bool bInSend)
{
	Connection->Poll();
	auto Bytes = Connection->Receive();
	if (Bytes.size() > MaxBufferedChannelFrames * (MaxFrame + ChannelFramePrefixBytes) - Input.size())
	{
		throw FTransportError(TransportErrors::ProtocolError, "Buffered input limit exceeded");
	}
	Input.insert(Input.end(), Bytes.begin(), Bytes.end());
	ValidatePrefix();
	if (bInSend && !Output.empty() && Connection->State() == ETransportState::Connected)
	{
		const auto& Front = Output.front();
		const auto Count =
		    std::min({Connection->WriteCapacity(), Front.size() - OutputOffset, ChannelWriteBytesPerPoll});
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
		throw FTransportError(TransportErrors::ProtocolError, "Output frame exceeds channel limits");
	}
	if (Output.size() >= MaxQueuedChannelFrames ||
	    InMessage.size() + ChannelFramePrefixBytes >
	        MaxBufferedChannelFrames * (MaxFrame + ChannelFramePrefixBytes) - OutputBytes)
	{
		throw FTransportError(TransportErrors::Backpressure, "Peer output queue is full");
	}
	FTransportBytes Bytes(ChannelFramePrefixBytes + InMessage.size());
	for (std::size_t Index = 0; Index < ChannelFramePrefixBytes; ++Index)
	{
		Bytes[Index] = std::byte((InMessage.size() >> ((ChannelFramePrefixBytes - 1 - Index) * 8)) & 255);
	}
	std::memcpy(Bytes.data() + ChannelFramePrefixBytes, InMessage.data(), InMessage.size());
	OutputBytes += Bytes.size();
	Output.push_back(std::move(Bytes));
}

std::optional<std::string> FAutomationChannel::Receive()
{
	ValidatePrefix();
	if (Input.size() < ChannelFramePrefixBytes || Input.size() < FrameSize(Input) + ChannelFramePrefixBytes)
	{
		return {};
	}
	const auto Size = FrameSize(Input);
	std::string Message(reinterpret_cast<const char*>(Input.data() + ChannelFramePrefixBytes), Size);
	Input.erase(Input.begin(), Input.begin() + Size + ChannelFramePrefixBytes);
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
