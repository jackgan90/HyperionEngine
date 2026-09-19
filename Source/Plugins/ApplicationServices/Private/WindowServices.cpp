#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
namespace
{
class FWindowServicesPlugin final : public FPlugin
{
public:
	explicit FWindowServicesPlugin(FWindowServiceOptions InOptions) : Options(std::move(InOptions))
	{
	}

	void Start(FPluginContext& InContext) override
	{
		Window = std::make_unique<FWindow>(Options.Title, Options.Size, Options.bHidden);
		if (Options.bDarkTitleBar && !Window->SetDarkTitleBar(true))
		{
			Log(ELogLevel::Warning, "Native dark title bar is unavailable on this platform");
		}
		InContext.Provide(*Window);
	}

	void Update(const FPluginUpdate&) override
	{
		Window->Poll();
	}

	void Stop() noexcept override
	{
		Window.reset();
	}

private:
	FWindowServiceOptions Options;
	std::unique_ptr<FWindow> Window;
};
} // namespace

void RegisterWindowServices(FPluginRegistry& InRegistry, FWindowServiceOptions InOptions)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "window";
	Descriptor.Provides = {typeid(FWindow)};
	Descriptor.After = {"renderdoc"};
	Descriptor.Create = [Options = std::move(InOptions)]
	{
		return std::make_unique<FWindowServicesPlugin>(Options);
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
