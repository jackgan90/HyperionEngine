#include "ContentRootOperations.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
class FSceneAutomationPlugin final : public FPlugin
{
public:
	void Start(FPluginContext& InContext) override
	{
		auto& Catalog = InContext.Require<FOperationCatalog>();
		InContext.Defer(
		    [&Catalog]
		    {
			    Catalog.UnregisterOwner("automation-scene");
		    });
		RegisterSceneOperations(Catalog, InContext.Find<FSceneEditDocument>());
		RegisterContentRootOperations(Catalog, InContext.Find<FContentRootService>(), "automation-scene", false);
	}
};
} // namespace

void RegisterSceneAutomation(FPluginRegistry& InRegistry)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "automation-scene";
	Descriptor.Dependencies = {"automation-catalog"};
	Descriptor.Before = {"automation-session"};
	Descriptor.Requires = {typeid(FOperationCatalog)};
	Descriptor.Optional = {typeid(FSceneEditDocument), typeid(FContentRootService)};
	Descriptor.Create = []
	{
		return std::make_unique<FSceneAutomationPlugin>();
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
