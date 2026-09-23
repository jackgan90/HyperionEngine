#include "Hyperion/Transport/MemoryTransport.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace Hyperion
{
namespace
{
void Check(bool bInCondition)
{
	if (!bInCondition)
	{
		throw std::runtime_error("Transport contract failed");
	}
}

void Exercise(FTransportRegistry& InRegistry, const FTransportAddress& InAddress)
{
	auto Listener = InRegistry.Listen(InAddress);
	auto Client = InRegistry.Connect(InAddress);
	std::unique_ptr<ITransportConnection> Server;
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (!Server && std::chrono::steady_clock::now() < Deadline)
	{
		Server = Listener->Accept();
	}
	Check(Server != nullptr);
	Check(Client->Peer().bCurrentUser && Server->Peer().bAuthenticated);
	Check(!Client->Peer().Principal.empty() && !Client->Peer().Authentication.empty());
	const FTransportBytes Input(20000, std::byte{37});
	Check(Client->Send(Input));
	Check(Server->Send({std::byte{91}}));
	FTransportBytes Received;
	FTransportBytes Reply;
	while ((Received.size() < Input.size() || Reply.empty()) && std::chrono::steady_clock::now() < Deadline)
	{
		Client->Poll();
		Server->Poll();
		auto Fragment = Server->Receive();
		Received.insert(Received.end(), Fragment.begin(), Fragment.end());
		auto Response = Client->Receive();
		Reply.insert(Reply.end(), Response.begin(), Response.end());
	}
	Check(Received == Input && Reply == FTransportBytes{std::byte{91}});
	Server->Poll(); // A native read can now be pending during Close.
	Server->Close();
	Server->Close();
	while (Client->State() == ETransportState::Connected && std::chrono::steady_clock::now() < Deadline)
	{
		Client->Poll();
	}
	Check(Client->State() != ETransportState::Connected);
	Listener->Close();
	Check(!Listener->Accept());
}

void MemoryLimits()
{
	FTransportRegistry Registry;
	Registry.Register(MakeMemoryTransport(4, 1));
	auto Listener = Registry.Listen({"memory", "bounded"});
	auto Client = Registry.Connect({"memory", "bounded"});
	auto Server = Listener->Accept();
	Check(!Client->Send(FTransportBytes(5)));
	Check(Client->Send(FTransportBytes(4)));
	Check(!Client->Send(FTransportBytes(1)));
	Check(Server->Receive().size() == 1);
	Check(Client->Send(FTransportBytes(1)));
	bool bRejected{};
	try
	{
		Registry.Connect({"tls", "device"});
	}
	catch (const FTransportError& Error)
	{
		bRejected = Error.GetCode() == "unsupported_transport";
	}
	Check(bRejected);
}

void ClosePendingWrite(FTransportRegistry& InRegistry, const FTransportAddress& InAddress)
{
	auto Listener = InRegistry.Listen(InAddress);
	auto Client = InRegistry.Connect(InAddress);
	auto Server = Listener->Accept();
	Check(Server != nullptr);
	Check(Client->Send(FTransportBytes(4 * 1024 * 1024, std::byte{7})));
	// Do not read on the peer: subsequent native writes must remain pending behind the pipe buffer.
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		Client->Poll();
	}
	Check(Client->HasPendingWrites());
	Client->Close();
	Server->Close();
	Listener->Close();
}
} // namespace
} // namespace Hyperion

int main()
{
	try
	{
		Hyperion::FTransportRegistry Registry;
		Registry.Register(Hyperion::MakeMemoryTransport());
		Hyperion::Exercise(Registry, {"memory", "test"});
		Hyperion::MemoryLimits();
		Hyperion::RegisterLocalTransport(Registry);
		const auto Unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		Hyperion::Exercise(Registry, Hyperion::LocalTransportAddress("test-" + Unique));
		Hyperion::ClosePendingWrite(Registry, Hyperion::LocalTransportAddress("write-" + Unique));
		std::cout << "Transport contracts passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
