#include "AssetOperations.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"

namespace Hyperion
{
namespace
{
class FAssetAutomationPlugin final : public FPlugin
{
public:
	void Start(FPluginContext& InContext) override
	{
		auto& Catalog = InContext.Require<FOperationCatalog>();
		InContext.Defer(
		    [&Catalog]
		    {
			    Catalog.UnregisterOwner("automation-assets");
		    });
		if (auto* Assets = InContext.Find<FAssetService>())
		{
			auto* Roots = InContext.Find<FContentRootService>();
			Provider = std::make_unique<FAssetAutomation>(*Assets, InContext.Require<FTaskSystem>(), Roots);
			if (Roots)
			{
				InContext.Defer(
				    [this, Roots]
				    {
					    Roots->UnregisterParticipant(*Provider);
				    });
				Roots->RegisterParticipant(*Provider);
			}
		}
		RegisterAssetOperations(Catalog, Provider.get());
		RegisterContentRootOperations(Catalog, InContext.Find<FContentRootService>());
	}

	void Quiesce() noexcept override
	{
		if (Provider)
		{
			Provider->Drain();
		}
	}

	void Stop() noexcept override
	{
		Provider.reset();
	}

private:
	std::unique_ptr<FAssetAutomation> Provider;
};
} // namespace

void RegisterAssetAutomation(FPluginRegistry& InRegistry)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "automation-assets";
	Descriptor.Dependencies = {"automation-catalog"};
	Descriptor.Before = {"automation-session"};
	Descriptor.Requires = {typeid(FOperationCatalog), typeid(FTaskSystem)};
	Descriptor.Optional = {typeid(FAssetService), typeid(FContentRootService)};
	Descriptor.Create = []
	{
		return std::make_unique<FAssetAutomationPlugin>();
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
