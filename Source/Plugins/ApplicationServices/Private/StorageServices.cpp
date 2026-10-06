#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/Config/StorageSettings.h"
#include "Hyperion/Content/ContentRootService.h"

namespace Hyperion
{
namespace
{
class FStoragePlugin final : public FPlugin
{
public:
	explicit FStoragePlugin(std::shared_ptr<FStorageSettings> InStorage) : Storage(std::move(InStorage))
	{
	}

	void Start(FPluginContext& InContext) override
	{
		if (auto* Content = InContext.Find<FContentRootService>())
		{
			InContext.Defer(
			    [this]
			    {
				    Storage->SetProtectedDirectoryQuery({});
			    });
			Storage->SetProtectedDirectoryQuery(
			    [Content]
			    {
				    const auto Root = Content->Directory();
				    return Root.empty() ? std::vector<std::filesystem::path>{} : std::vector{Root};
			    });
		}
		InContext.Provide(*Storage);
	}

private:
	std::shared_ptr<FStorageSettings> Storage;
};
} // namespace

void RegisterStorageServices(FPluginRegistry& InRegistry, std::shared_ptr<FStorageSettings> InStorage)
{
	if (!InStorage)
	{
		throw std::invalid_argument("Storage provider requires a service");
	}
	FPluginDescriptor Descriptor;
	Descriptor.Id = "storage";
	Descriptor.Provides = {typeid(FStorageSettings)};
	Descriptor.Optional = {typeid(FContentRootService)};
	Descriptor.Create = [Storage = std::move(InStorage)]
	{
		return std::make_unique<FStoragePlugin>(Storage);
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
