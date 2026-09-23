#pragma once
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace Hyperion
{
using FTransportBytes = std::vector<std::byte>;

struct FTransportAddress
{
	std::string Scheme;
	std::string Address;
	bool operator==(const FTransportAddress&) const = default;
};

struct FTransportPeer
{
	// Verified provider facts, never values copied from the application handshake.
	bool bAuthenticated{};
	bool bLocal{};
	bool bCurrentUser{};
	bool bEncrypted{};
	std::string Principal;
	std::string Authentication;
};

enum class ETransportState
{
	Connecting,
	Connected,
	Closed,
	Failed
};

class FTransportError : public std::runtime_error
{
public:
	FTransportError(std::string InCode, std::string InMessage);
	const std::string& GetCode() const noexcept;

private:
	std::string Code;
};

// Owner-thread, nonblocking progress API. Native pending IO only borrows provider-owned buffers.
// Send takes ownership on acceptance; false means backpressure and has no effects.
// Receive returns a byte fragment, not a message. Empty means no currently buffered bytes.
// Poll reports EOF/failure through State; drain Receive before interpreting terminal state.
// Successful Send is not peer acknowledgement. Close joins/cancels native IO before freeing it.
class ITransportConnection
{
public:
	virtual ~ITransportConnection() = default;
	virtual void Poll() = 0;
	virtual bool Send(FTransportBytes InBytes) = 0;
	virtual std::size_t WriteCapacity() const noexcept = 0;
	virtual bool HasPendingWrites() const noexcept = 0;
	virtual FTransportBytes Receive() = 0;
	virtual ETransportState State() const noexcept = 0;
	virtual std::string Error() const = 0;
	virtual const FTransportPeer& Peer() const noexcept = 0;
	virtual void Close() noexcept = 0;
};

class ITransportListener
{
public:
	virtual ~ITransportListener() = default;
	virtual std::unique_ptr<ITransportConnection> Accept() = 0;
	virtual void Close() noexcept = 0;
};

class ITransportProvider
{
public:
	virtual ~ITransportProvider() = default;
	virtual std::string Scheme() const = 0;
	virtual std::unique_ptr<ITransportConnection> Connect(const std::string& InAddress) = 0;
	virtual std::unique_ptr<ITransportListener> Listen(const std::string& InAddress) = 0;
};

class FTransportRegistry
{
public:
	void Register(std::unique_ptr<ITransportProvider> InProvider);
	std::unique_ptr<ITransportConnection> Connect(const FTransportAddress& InAddress) const;
	std::unique_ptr<ITransportListener> Listen(const FTransportAddress& InAddress) const;

private:
	ITransportProvider& Find(const std::string& InScheme) const;
	std::map<std::string, std::unique_ptr<ITransportProvider>> Providers;
};

// Platform composition hook. Only the current platform's compiled local provider is registered.
void RegisterLocalTransport(FTransportRegistry& InRegistry);
FTransportAddress LocalTransportAddress(const std::string& InInstance);
std::filesystem::path LocalDiscoveryDirectory();
} // namespace Hyperion
