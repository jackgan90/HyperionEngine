#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Scene/SceneManifest.h"

namespace Hyperion
{
namespace
{
class FAssetServicesPlugin final : public FPlugin
{
public:
	explicit FAssetServicesPlugin(FAssetServiceOptions InOptions) : Options(std::move(InOptions))
	{
	}

	void Start(FPluginContext& InContext) override
	{
		Files = LoadContentMounts(Options.Mounts);
		IO = std::make_unique<FIOService>(InContext.Require<FTaskSystem>(), Files);
		Assets = std::make_unique<FAssetService>(*IO);
		RegisterSceneAssetTypes(Assets->Types());
		for (const auto& Mount : Files->GetMounts())
		{
			if (!IO->FileSystem()->Exists(Mount.Root / "Catalog.hasset"))
			{
				continue;
			}
			try
			{
				const auto Catalog = Assets->LoadAsync<FAssetCatalog>(Mount.Root / "Catalog.hasset")
				                         .Get(InContext.Require<FTaskSystem>());
				Assets->AddCatalog(*Catalog, Mount.Root);
			}
			catch (const std::exception& Failure)
			{
				if (Options.bRequireCatalogs)
				{
					throw;
				}
				Log(ELogLevel::Warning, "Catalog unavailable: " + Mount.Root.string() + ": " + Failure.what());
			}
		}
		InContext.Provide(*Files);
		InContext.Provide(*IO);
		InContext.Provide(*Assets);
	}

	void Stop() noexcept override
	{
		if (Assets)
		{
			Assets->Drain();
		}
		Assets.reset();
		IO.reset();
		Files.reset();
	}

private:
	FAssetServiceOptions Options;
	std::shared_ptr<FMountedFileSystem> Files;
	std::unique_ptr<FIOService> IO;
	std::unique_ptr<FAssetService> Assets;
};
} // namespace

void RegisterAssetServices(FPluginRegistry& InRegistry, FAssetServiceOptions InOptions)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "assets";
	Descriptor.Requires = {typeid(FTaskSystem)};
	Descriptor.Provides = {typeid(FMountedFileSystem), typeid(FIOService), typeid(FAssetService)};
	Descriptor.Create = [Options = std::move(InOptions)]
	{
		return std::make_unique<FAssetServicesPlugin>(Options);
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
