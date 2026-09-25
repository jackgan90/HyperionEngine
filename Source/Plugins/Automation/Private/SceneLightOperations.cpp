#include "Hyperion/Renderer/SceneLightControls.h"
#include "SceneOperations.h"

namespace Hyperion
{
void RegisterSceneLightControls(FOperationCatalog& InCatalog, ISceneLightControls* InLights)
{
	FOperationInfo Info;
	Info.Id = "light.main.get";
	Info.Owner = "automation-scene";
	Info.Summary = "Read scene main directional light";
	Info.Description = "Edits the scene main directional light through the shared document. Read before setting.";
	Info.Effects = "Reads scene light state.";
	Info.Completion = "Main snapshot.";
	Info.bReadOnly = true;
	Info.Unavailable =
	    InLights ? "" : "Main light control provider unavailable; use scene component operations where supported.";
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FSceneMainLight>(Info,
	                                                                     [InLights](const auto&)
	                                                                     {
		                                                                     try
		                                                                     {
			                                                                     return InLights->MainLight();
		                                                                     }
		                                                                     catch (const FSceneEditError& Error)
		                                                                     {
			                                                                     throw FAutomationError(Error.Code,
			                                                                                            Error.what());
		                                                                     }
	                                                                     }));
	Info.Id = "light.main.set";
	Info.Summary = "Edit scene main directional light";
	Info.Effects = "Commits one undoable scene edit using shared validation. Save the document explicitly.";
	Info.bReadOnly = false;
	const FSceneMainLight Example{1, {1, 0, 1}, {}, {0, 1, 0}};
	Info.Example = WriteRecordWire(RecordType<FSceneMainLight>(), &Example);
	InCatalog.Register(MakeOperation<FSceneMainLight, FSceneMainLight>(Info,
	                                                                   [InLights](const auto& InRequest)
	                                                                   {
		                                                                   try
		                                                                   {
			                                                                   InLights->SetMainLight(InRequest);
			                                                                   return InLights->MainLight();
		                                                                   }
		                                                                   catch (const FSceneEditError& Error)
		                                                                   {
			                                                                   throw FAutomationError(Error.Code,
			                                                                                          Error.what());
		                                                                   }
	                                                                   }));
}
} // namespace Hyperion
