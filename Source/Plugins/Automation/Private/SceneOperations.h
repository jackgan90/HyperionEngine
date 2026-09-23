#pragma once
#include "Hyperion/Automation/Catalog.h"
#include "Hyperion/SceneEditing/SceneRequests.h"

namespace Hyperion
{
class ISceneViewport;
class IRenderOutput;
class IRenderDiagnostics;
class IShadowControls;
class ISceneLightControls;
void RegisterSceneLightControls(FOperationCatalog& InCatalog, ISceneLightControls* InLights);
void RegisterShadowControls(FOperationCatalog& InCatalog, IShadowControls* InShadows);
void RegisterRenderDiagnostics(FOperationCatalog& InCatalog, IRenderDiagnostics* InDiagnostics);
class IRenderCaptureControl;
class FGui;
class IApplicationSettings;
class IApplicationClose;
void RegisterApplicationClose(FOperationCatalog& InCatalog, IApplicationClose* InHost);
void RegisterApplicationSettings(FOperationCatalog& InCatalog, IApplicationSettings* InSettings);
void RegisterProfilingOperations(FOperationCatalog& InCatalog, IApplicationSettings* InSettings);
void RegisterRenderCapture(FOperationCatalog& InCatalog, IRenderCaptureControl* InCapture, FGui* InGui);
void RegisterRenderOutput(FOperationCatalog& InCatalog, IRenderOutput* InOutput);
class IScenePlacement;
void RegisterPlacementOperations(FOperationCatalog& InCatalog, IScenePlacement* InPlacement);
void RegisterViewportOperations(FOperationCatalog& InCatalog, ISceneViewport* InView, FSceneEditDocument* InDocument);
class ISceneDocumentHost;
void RegisterSceneHostOperations(FOperationCatalog& InCatalog, ISceneDocumentHost* InHost);
void RegisterSceneAuthoring(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument);
void RegisterSceneComponents(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument);
void RegisterSceneOperations(FOperationCatalog& InCatalog, FSceneEditDocument* InDocument);
} // namespace Hyperion
