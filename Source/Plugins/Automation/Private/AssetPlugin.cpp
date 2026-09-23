#include "AssetOperations.h"
#include "ContentRootOperations.h"
#include "Hyperion/AssetEditing/AssetPreview.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include "Hyperion/SceneEditing/SceneDocumentHost.h"
#include "ImportOperations.h"

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
			auto* Workspace = InContext.Find<IAssetWorkspace>();
			Provider = std::make_unique<FAssetAutomation>(*Assets, InContext.Require<FTaskSystem>(), Roots, Workspace);
			if (Roots && !Workspace)
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
		RegisterAssetPreviews(Catalog, InContext.Find<IAssetPreviewWorkspace>());
		if (auto* IO = InContext.Find<FIOService>(); IO && Provider && InContext.Find<FContentRootService>())
		{
			auto& Roots = *InContext.Find<FContentRootService>();
			Imports = std::make_unique<FImportAutomation>(*IO, *InContext.Find<FAssetService>(), Roots);
			InContext.Defer(
			    [this, &Roots]
			    {
				    Roots.UnregisterParticipant(*Imports);
			    });
			Roots.RegisterParticipant(*Imports);
			Imports->Register(Catalog);
		}
		const auto* Host = InContext.Find<ISceneDocumentHost>();
		const bool bMutableRoot = Host ? Host->SupportsContentTransitions() : !InContext.Find<FSceneEditDocument>();
		RegisterContentRootOperations(Catalog, InContext.Find<FContentRootService>(), "automation-assets",
		                              bMutableRoot);
		RegisterContentQueries(Catalog, InContext.Find<FAssetService>(), InContext.Find<FContentRootService>(),
		                       "automation-assets");
	}

	void Quiesce() noexcept override
	{
		if (Imports)
		{
			Imports->Drain();
		}
		if (Provider)
		{
			Provider->Drain();
		}
	}

	void Stop() noexcept override
	{
		Imports.reset();
		Provider.reset();
	}

private:
	std::unique_ptr<FAssetAutomation> Provider;
	std::unique_ptr<FImportAutomation> Imports;
};
} // namespace

void RegisterAssetAutomation(FPluginRegistry& InRegistry)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "automation-assets";
	Descriptor.Dependencies = {"automation-catalog"};
	Descriptor.Before = {"automation-session"};
	Descriptor.Requires = {typeid(FOperationCatalog), typeid(FTaskSystem)};
	Descriptor.Optional = {typeid(FAssetService),     typeid(FContentRootService),    typeid(IAssetWorkspace),
	                       typeid(FIOService),        typeid(IAssetPreviewWorkspace), typeid(FSceneEditDocument),
	                       typeid(ISceneDocumentHost)};
	Descriptor.Create = []
	{
		return std::make_unique<FAssetAutomationPlugin>();
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
