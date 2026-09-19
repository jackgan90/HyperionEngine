#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/Renderer/RenderFeatures.h"

namespace Hyperion
{
namespace
{
class FContactShadowPlugin final : public FPlugin
{
public:
	void Start(FPluginContext& InContext) override
	{
		auto& Features = InContext.Require<FRenderFeatureRegistry>();
		Features.Add("contact-shadows", MakeContactShadowFeature);
		try
		{
			InContext.Defer(
			    [&Features]
			    {
				    Features.Remove("contact-shadows");
			    });
		}
		catch (...)
		{
			Features.Remove("contact-shadows");
			throw;
		}
	}
};
} // namespace

void RegisterContactShadowServices(FPluginRegistry& InRegistry)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "contact-shadows";
	Descriptor.Dependencies = {"graphics"};
	Descriptor.Requires = {typeid(FRenderFeatureRegistry)};
	Descriptor.Create = []
	{
		return std::make_unique<FContactShadowPlugin>();
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
