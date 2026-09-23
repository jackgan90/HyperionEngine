#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/Automation/Connections.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <source_location>
#include <thread>

namespace
{
using namespace Hyperion;

void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Local automation check failed: " + std::to_string(InLocation.line()));
	}
}

const FArchiveNode& Field(const FArchiveNode& InValue, const std::string& InName)
{
	return std::get<FArchiveNode::FObject>(InValue.Value).at(InName);
}

FArchiveNode Await(FEndpointRequest InRequest, FConnectionManager& InClient, FApplicationHost& InHost)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	do
	{
		InHost.GetTasks().PumpMain();
		InHost.GetPlugins().Update({});
		InClient.Poll();
		if (auto Result = InRequest.Poll())
		{
			return *Result;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	} while (std::chrono::steady_clock::now() < Deadline);
	throw std::runtime_error("Local automation request timed out");
}

void CheckExitAdmission()
{
	FApplicationHost Host(1, 1);
	FPluginRegistry Registry;
	RegisterAutomationServices(Registry);
	const auto Application = "ExitAdmission-" + CreateAutomationIdentity();
	RegisterAutomationLocal(Registry, Application);
	Host.Start(Registry, {{"automation-local"}});
	Check(Host.GetPlugins().IsActive("automation-local"));
	FTransportRegistry Transports;
	RegisterLocalTransport(Transports);
	FLocalTargetDiscovery Discovery;
	FCurrentUserAccessPolicy Access;
	FConnectionManager Client(Transports, Discovery, Access);
	const auto Targets = Discovery.List();
	const auto Found = std::find_if(Targets.begin(), Targets.end(),
	                                [&](const FAutomationTarget& InTarget)
	                                {
		                                return InTarget.Application == Application;
	                                });
	Check(Found != Targets.end());
	const auto Connected = Await(Client.Connect(ParseJson("{\"instance\":\"" + Found->Instance + "\"}")), Client, Host);
	Check(ReadValue<std::string>(Field(Connected, "status")) == "completed");
	const auto Connection = ReadValue<std::string>(Field(Field(Connected, "result"), "connection"));
	const auto Info = Await(Client.Request(Connection, "engine.info", ParseJson("{}")), Client, Host);
	Check(ReadValue<std::string>(Field(Field(Info, "target"), "instance")) == Found->Instance);
	// The request is already queued when an earlier host plugin accepts the close decision.
	auto Pending = Client.Request(Connection, "engine.info", ParseJson("{}"));
	Client.Poll();
	Host.GetServices().Require<FApplicationControl>().RequestExit();
	const auto Rejected = Await(std::move(Pending), Client, Host);
	Check(ReadValue<std::string>(Field(Rejected, "status")) == "failed");
	const auto Remaining = Discovery.List();
	Check(std::none_of(Remaining.begin(), Remaining.end(),
	                   [&](const FAutomationTarget& InTarget)
	                   {
		                   return InTarget.Instance == Found->Instance;
	                   }));
	Host.Stop();
	Host.GetServices().Require<FApplicationControl>().RethrowFailure();
}
} // namespace

int main()
{
	try
	{
		CheckExitAdmission();
		std::cout << "Host exit withdraws discovery and rejects queued requests before quiesce\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
