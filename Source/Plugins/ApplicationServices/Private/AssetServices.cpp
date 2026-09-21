#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/ApplicationServices/ContentRootService.h"
#include "Hyperion/Assets/AssetRegistry.h"
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
		Content = std::make_unique<FContentRootService>(InContext.Require<FTaskSystem>(), *Files, *Assets);
		if (Options.RestoredGameRoot)
		{
			try
			{
				Content->Commit(Content->Prepare(*Options.RestoredGameRoot));
			}
			catch (const std::exception& Failure)
			{
				Content->StartupError = "Could not restore asset root: " + std::string(Failure.what());
				auto Mounts = Files->GetMounts();
				std::erase_if(Mounts,
				              [](const auto& InMount)
				              {
					              return InMount.Root == "/Game";
				              });
				FMountedFileSystem Unmounted(std::move(Mounts));
				Files->ReplaceExclusive(Unmounted);
			}
		}
		for (const auto& Mount : Files->GetMounts())
		{
			try
			{
				const auto Discovery = DispatchAsync<FAssetDiscovery>(InContext.Require<FTaskSystem>(), {EDomain::Io},
				                                                      [Storage = Files, Root = Mount.Root]
				                                                      {
					                                                      return DiscoverAssets(*Storage, Root);
				                                                      })
				                           .Get(InContext.Require<FTaskSystem>());
				Assets->AddCatalog(BuildAssetCatalog(Discovery->Entries), Mount.Root);
				for (const auto& [Path, Error] : Discovery->Errors)
				{
					Log(ELogLevel::Warning, "Asset unavailable: " + Path.string() + ": " + Error);
				}
			}
			catch (const std::exception& Failure)
			{
				if (Options.bRequireDiscovery)
				{
					throw;
				}
				Log(ELogLevel::Warning, "Asset discovery failed: " + Mount.Root.string() + ": " + Failure.what());
			}
		}
		InContext.Provide(*Files);
		InContext.Provide(*IO);
		InContext.Provide(*Assets);
		InContext.Provide(*Content);
	}

	void Stop() noexcept override
	{
		if (Assets)
		{
			Assets->Drain();
		}
		Assets.reset();
		Content.reset();
		IO.reset();
		Files.reset();
	}

private:
	FAssetServiceOptions Options;
	std::shared_ptr<FMountedFileSystem> Files;
	std::unique_ptr<FIOService> IO;
	std::unique_ptr<FAssetService> Assets;
	std::unique_ptr<FContentRootService> Content;
};
} // namespace

void RegisterAssetServices(FPluginRegistry& InRegistry, FAssetServiceOptions InOptions)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "assets";
	Descriptor.Requires = {typeid(FTaskSystem)};
	Descriptor.Provides = {typeid(FMountedFileSystem), typeid(FIOService), typeid(FAssetService),
	                       typeid(FContentRootService)};
	Descriptor.Create = [Options = std::move(InOptions)]
	{
		return std::make_unique<FAssetServicesPlugin>(Options);
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
