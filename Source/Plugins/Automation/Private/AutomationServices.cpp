#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include <chrono>
#include <thread>

namespace Hyperion
{
namespace
{
class FAutomationCatalogPlugin final : public FPlugin
{
public:
	void Start(FPluginContext& InContext) override
	{
		InContext.Provide(Catalog);
	}

private:
	FOperationCatalog Catalog;
};

class FAutomationSessionPlugin final : public FPlugin
{
public:
	void Start(FPluginContext& InContext) override
	{
		Tasks = &InContext.Require<FTaskSystem>();
		Control = &InContext.Require<FApplicationControl>();
		auto& Catalog = InContext.Require<FOperationCatalog>();
		Catalog.Seal();
		Session = std::make_unique<FAutomationSession>(Catalog);
		Endpoint = std::make_unique<FAutomationEndpoint>(*Session);
		InContext.Provide(*Session);
		InContext.Provide(*Endpoint);
	}

	void Update(const FPluginUpdate&) override
	{
		Session->Poll();
	}

	void Quiesce() noexcept override
	{
		if (!Session)
		{
			return;
		}
		try
		{
			Session->StopAdmission();
			while (Session->PendingCount())
			{
				Tasks->PumpMain();
				Session->Poll();
				if (Session->PendingCount())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}
		}
		catch (...)
		{
			Control->ReportFailure(std::current_exception());
		}
	}

	void Stop() noexcept override
	{
		Endpoint.reset();
		Session.reset();
	}

private:
	FTaskSystem* Tasks{};
	FApplicationControl* Control{};
	std::unique_ptr<FAutomationSession> Session;
	std::unique_ptr<FAutomationEndpoint> Endpoint;
};
} // namespace

void RegisterAutomationServices(FPluginRegistry& InRegistry)
{
	FPluginDescriptor Catalog;
	Catalog.Id = "automation-catalog";
	Catalog.Provides = {typeid(FOperationCatalog)};
	Catalog.Create = []
	{
		return std::make_unique<FAutomationCatalogPlugin>();
	};
	InRegistry.Add(std::move(Catalog));
	FPluginDescriptor Session;
	Session.Id = "automation-session";
	Session.Dependencies = {"automation-catalog"};
	Session.Requires = {typeid(FOperationCatalog), typeid(FTaskSystem), typeid(FApplicationControl)};
	Session.Provides = {typeid(FAutomationSession), typeid(FAutomationEndpoint)};
	Session.Create = []
	{
		return std::make_unique<FAutomationSessionPlugin>();
	};
	InRegistry.Add(std::move(Session));
}
} // namespace Hyperion
