#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::DrawRenderSettings()
{
	if (!bShowRenderSettings)
	{
		return;
	}
	if (Gui->BeginWindow("Render settings", bShowRenderSettings, {440, 300}))
	{
		auto Candidate = Rendering;
		const std::array<std::string, 2> Pipelines{"Deferred", "Forward"};
		const std::array<std::string, 2> Formats{"Compact", "High precision"};
		std::size_t PipelineIndex = Candidate.Pipeline == "forward" ? 1 : 0;
		std::size_t FormatIndex = Candidate.GBuffer == "high" ? 1 : 0;
		Gui->BeginPropertyRow("Pipeline");
		bool bChanged = Gui->Combo("##Pipeline", Pipelines, PipelineIndex);
		InspectionBounds["render/pipeline"] = Gui->LastItemBounds();
		Gui->EndPropertyRow();
		Gui->BeginPropertyRow("GBuffer layout");
		Gui->BeginDisabled(PipelineIndex == 1);
		bChanged |= Gui->Combo("##GBuffer", Formats, FormatIndex);
		Gui->EndDisabled();
		Gui->EndPropertyRow();
		Candidate.Pipeline = PipelineIndex ? "forward" : "deferred";
		Candidate.GBuffer = FormatIndex ? "high" : "compact";
		bChanged |= Gui->Checkbox("Clustered lighting", Candidate.bClusteredLighting);
		bChanged |= Gui->Checkbox("VSync", Candidate.bVsync);
		bChanged |= Gui->Checkbox("Reversed Z (restart required)", Candidate.bReversedZ);
		Gui->TextWrapped(
		    "Save settings and restart to apply the depth convention. Other switches apply to subsequent frames.");
		try
		{
			if (bChanged)
			{
				SetRenderSettings(RenderSettingsRevision, Candidate);
				RenderSettingsError.clear();
			}
			if (Gui->Button("Save render settings"))
			{
				SaveRenderSettings({});
				RenderSettingsError.clear();
			}
		}
		catch (const std::exception& Failure)
		{
			RenderSettingsError = Failure.what();
		}
		if (!RenderSettingsError.empty())
		{
			Gui->TextWrapped(RenderSettingsError);
		}
	}
	Gui->EndWindow();
}
} // namespace Hyperion
