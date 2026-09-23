#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/Automation/Connections.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Core/Core.h"
#include <thread>

namespace Hyperion
{
namespace
{
class FLocalAutomationPlugin final : public FPlugin
{
public:
	explicit FLocalAutomationPlugin(std::string InApplication) : Application(std::move(InApplication))
	{
	}

	void Start(FPluginContext& InContext) override
	{
		Tasks = &InContext.Require<FTaskSystem>();
		Control = &InContext.Require<FApplicationControl>();
		RegisterLocalTransport(Transports);
		Info = {CreateAutomationIdentity(), Application, HYP_AUTOMATION_BUILD, {}};
		Info.Address = LocalTransportAddress(Info.Instance);
		InContext.Defer(
		    [this]
		    {
			    Discovery.Withdraw(Info.Instance);
		    });
		Server = std::make_unique<FAutomationServer>(InContext.Require<FOperationCatalog>(), Info,
		                                             Transports.Listen(Info.Address), Access);
		Discovery.Publish(Info);
		Log(ELogLevel::Info, "Automation target: " + Info.Instance + " (" + Application + ")");
	}

	void Update(const FPluginUpdate&) override
	{
		try
		{
			if (Control->IsExitRequested())
			{
				Server->StopAdmission();
				Discovery.Withdraw(Info.Instance);
			}
			Server->Poll();
		}
		catch (const std::exception& Error)
		{
			// Communication failure isolates the listener; admitted jobs remain owned until shutdown.
			Server->StopAdmission();
			Discovery.Withdraw(Info.Instance);
			Log(ELogLevel::Error, std::string("Automation listener stopped: ") + Error.what());
		}
	}

	void Quiesce() noexcept override
	{
		Discovery.Withdraw(Info.Instance);
		if (!Server)
		{
			return;
		}
		try
		{
			Server->StopAdmission();
			while (Server->PendingCount())
			{
				Tasks->PumpMain();
				Server->Poll();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		catch (...)
		{
			Control->ReportFailure(std::current_exception());
		}
	}

	void Stop() noexcept override
	{
		Server.reset();
	}

private:
	std::string Application;
	FAutomationTarget Info;
	FTaskSystem* Tasks{};
	FApplicationControl* Control{};
	FTransportRegistry Transports;
	FCurrentUserAccessPolicy Access;
	FLocalTargetDiscovery Discovery;
	std::unique_ptr<FAutomationServer> Server;
};
} // namespace

void RegisterAutomationLocal(FPluginRegistry& InRegistry, std::string InApplication)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "automation-local";
	Descriptor.Dependencies = {"automation-session"};
	Descriptor.Requires = {typeid(FOperationCatalog), typeid(FTaskSystem), typeid(FApplicationControl)};
	Descriptor.Create = [Application = std::move(InApplication)]
	{
		return std::make_unique<FLocalAutomationPlugin>(Application);
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
