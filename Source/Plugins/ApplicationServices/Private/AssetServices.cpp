#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/Content/ContentRootService.h"
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
		Files = CreateContentFileSystem(Options.EngineContent);
		IO = std::make_unique<FIOService>(InContext.Require<FTaskSystem>(), Files);
		Assets = std::make_unique<FAssetService>(*IO);
		RegisterSceneAssetTypes(Assets->Types());
		Content = std::make_unique<FContentRootService>(InContext.Require<FTaskSystem>(), *Files, *Assets);
		if (Options.AssetRoot)
		{
			try
			{
				if (Options.AssetRoot->empty())
				{
					throw std::invalid_argument("Asset root must be a non-empty directory");
				}
				Content->Change(*Options.AssetRoot, Options.bReadOnly);
			}
			catch (const std::exception& Failure)
			{
				if (!Options.bRecoverInvalidRoot)
				{
					throw;
				}
				Content->StartupError = "Could not restore asset root: " + std::string(Failure.what());
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
