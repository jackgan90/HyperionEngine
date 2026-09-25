#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Config/ApplicationClose.h"
#include "Hyperion/Core/ProfilingControl.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderCaptureControl.h"
#include "Hyperion/Renderer/RenderDiagnostics.h"
#include "Hyperion/Renderer/RenderOutput.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/Renderer/SceneLightControls.h"
#include "Hyperion/Renderer/SceneViewport.h"
#include "Hyperion/Renderer/ShadowControls.h"
#include "Hyperion/SceneEditing/SceneDocumentHost.h"
#include "Hyperion/SceneEditing/ScenePlacement.h"
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
		RegisterViewportOperations(Catalog, InContext.Find<ISceneViewport>(), InContext.Find<FSceneEditDocument>());
		RegisterPlacementOperations(Catalog, InContext.Find<IScenePlacement>());
		RegisterRenderOutput(Catalog, InContext.Find<IRenderOutput>());
		RegisterRenderDiagnostics(Catalog, InContext.Find<IRenderDiagnostics>());
		RegisterShadowControls(Catalog, InContext.Find<IShadowControls>());
		RegisterSceneLightControls(Catalog, InContext.Find<ISceneLightControls>());
		RegisterRenderCapture(Catalog, InContext.Find<IRenderCaptureControl>(), InContext.Find<FGui>());
		RegisterApplicationClose(Catalog, InContext.Find<IApplicationClose>());
		RegisterProfilingOperations(Catalog, InContext.Find<IProfilingControl>());
		RegisterRenderSettings(Catalog, InContext.Find<IRenderSettings>());
		auto* Host = InContext.Find<ISceneDocumentHost>();
		RegisterSceneHostOperations(Catalog, Host);
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
	Descriptor.Optional = {typeid(FSceneEditDocument), typeid(IRenderSettings),       typeid(IProfilingControl),
	                       typeid(ISceneDocumentHost), typeid(ISceneViewport),        typeid(IScenePlacement),
	                       typeid(IRenderOutput),      typeid(IRenderCaptureControl), typeid(FGui),
	                       typeid(IRenderDiagnostics), typeid(IShadowControls),       typeid(ISceneLightControls),
	                       typeid(IApplicationClose)};
	Descriptor.Create = []
	{
		return std::make_unique<FSceneAutomationPlugin>();
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
