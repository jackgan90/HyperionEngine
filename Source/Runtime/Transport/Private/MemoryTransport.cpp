#include "Hyperion/Transport/MemoryTransport.h"
#include <algorithm>
#include <array>
#include <deque>

namespace Hyperion
{
namespace
{
struct FMemoryStream
{
	std::array<std::deque<std::byte>, 2> Buffers;
	std::array<bool, 2> Closed{};
	std::size_t Capacity{};
	std::size_t Fragment{};
};

class FMemoryConnection final : public ITransportConnection
{
public:
	FMemoryConnection(std::shared_ptr<FMemoryStream> InStream, std::size_t InSide)
	    : Stream(std::move(InStream)), Side(InSide)
	{
	}

	~FMemoryConnection() override
	{
		Close();
	}

	void Poll() override
	{
	}

	bool Send(FTransportBytes InBytes) override
	{
		if (Stream->Closed[0] || Stream->Closed[1])
		{
			throw FTransportError("disconnected", "Memory stream is closed");
		}
		auto& Buffer = Stream->Buffers[1 - Side];
		if (InBytes.size() > Stream->Capacity - Buffer.size())
		{
			return false;
		}
		Buffer.insert(Buffer.end(), InBytes.begin(), InBytes.end());
		return true;
	}

	FTransportBytes Receive() override
	{
		auto& Buffer = Stream->Buffers[Side];
		const auto Count = std::min(Buffer.size(), Stream->Fragment);
		FTransportBytes Bytes(Buffer.begin(), Buffer.begin() + Count);
		Buffer.erase(Buffer.begin(), Buffer.begin() + Count);
		return Bytes;
	}

	std::size_t WriteCapacity() const noexcept override
	{
		return !Stream->Closed[0] && !Stream->Closed[1] ? CapacityRemaining() : 0;
	}

	bool HasPendingWrites() const noexcept override
	{
		return false;
	}

	ETransportState State() const noexcept override
	{
		return Stream->Closed[Side] || (Stream->Closed[1 - Side] && Stream->Buffers[Side].empty())
		           ? ETransportState::Closed
		           : ETransportState::Connected;
	}

	std::string Error() const override
	{
		return {};
	}

	const FTransportPeer& Peer() const noexcept override
	{
		return PeerInfo;
	}

	void Close() noexcept override
	{
		Stream->Closed[Side] = true;
	}

private:
	std::size_t CapacityRemaining() const noexcept
	{
		return Stream->Capacity - Stream->Buffers[1 - Side].size();
	}

	std::shared_ptr<FMemoryStream> Stream;
	FTransportPeer PeerInfo{true, true, true, false, "test-user", "memory-test"};
	std::size_t Side{};
};

struct FMemoryAcceptor
{
	bool bClosed{};
	std::deque<std::unique_ptr<ITransportConnection>> Pending;
};

class FMemoryListener final : public ITransportListener
{
public:
	explicit FMemoryListener(std::shared_ptr<FMemoryAcceptor> InAcceptor) : Acceptor(std::move(InAcceptor))
	{
	}

	~FMemoryListener() override
	{
		Close();
	}

	std::unique_ptr<ITransportConnection> Accept() override
	{
		if (Acceptor->Pending.empty())
		{
			return {};
		}
		auto Connection = std::move(Acceptor->Pending.front());
		Acceptor->Pending.pop_front();
		return Connection;
	}

	void Close() noexcept override
	{
		Acceptor->bClosed = true;
		Acceptor->Pending.clear();
	}

private:
	std::shared_ptr<FMemoryAcceptor> Acceptor;
};

class FMemoryProvider final : public ITransportProvider
{
public:
	FMemoryProvider(std::size_t InCapacity, std::size_t InFragment) : Capacity(InCapacity), Fragment(InFragment)
	{
		if (!Capacity || !Fragment)
		{
			throw std::invalid_argument("Memory transport limits must be positive");
		}
	}

	std::string Scheme() const override
	{
		return "memory";
	}

	std::unique_ptr<ITransportConnection> Connect(const std::string& InAddress) override
	{
		const auto Acceptor = Acceptors[InAddress].lock();
		if (!Acceptor || Acceptor->bClosed || Acceptor->Pending.size() >= 32)
		{
			throw FTransportError("connection_failed", "Memory listener unavailable");
		}
		auto Stream = std::make_shared<FMemoryStream>();
		Stream->Capacity = Capacity;
		Stream->Fragment = Fragment;
		Acceptor->Pending.push_back(std::make_unique<FMemoryConnection>(Stream, 1));
		return std::make_unique<FMemoryConnection>(std::move(Stream), 0);
	}

	std::unique_ptr<ITransportListener> Listen(const std::string& InAddress) override
	{
		if (auto Existing = Acceptors[InAddress].lock(); Existing && !Existing->bClosed)
		{
			throw FTransportError("address_in_use", "Memory listener already exists");
		}
		auto Acceptor = std::make_shared<FMemoryAcceptor>();
		Acceptors[InAddress] = Acceptor;
		return std::make_unique<FMemoryListener>(std::move(Acceptor));
	}

private:
	std::size_t Capacity{};
	std::size_t Fragment{};
	std::map<std::string, std::weak_ptr<FMemoryAcceptor>> Acceptors;
};
} // namespace

std::unique_ptr<ITransportProvider> MakeMemoryTransport(std::size_t InCapacity, std::size_t InFragmentSize)
{
	return std::make_unique<FMemoryProvider>(InCapacity, InFragmentSize);
}
} // namespace Hyperion
