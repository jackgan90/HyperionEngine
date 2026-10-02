#include "EditorApplication.h"
#include "Hyperion/RasterOptions/RasterOptions.h"

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
		const auto Pipelines = SceneRenderPipelineOptions();
		const auto Formats = GBufferPresetOptions();
		static const auto PipelineLabels = RasterOptionLabels(Pipelines);
		static const auto FormatLabels = RasterOptionLabels(Formats);
		auto PipelineIndex = RasterOptionIndex(Pipelines, ParseSceneRenderPipeline(Candidate.Pipeline));
		auto FormatIndex = RasterOptionIndex(Formats, ParseGBufferPreset(Candidate.GBuffer));
		Gui->BeginPropertyRow("Pipeline");
		bool bChanged = Gui->Combo("##Pipeline", PipelineLabels, PipelineIndex,
		                           [&](std::size_t InIndex, FVec4 InBounds)
		                           {
			                           InspectionBounds["render/pipeline/" + std::string(Pipelines[InIndex].Token)] =
			                               InBounds;
		                           });
		InspectionBounds["render/pipeline"] = Gui->LastItemBounds();
		Gui->EndPropertyRow();
		if (bChanged)
		{
			Candidate.Pipeline = ToSceneRenderPipelineToken(RasterOptionIdentity(Pipelines, PipelineIndex));
		}
		Gui->BeginPropertyRow("GBuffer layout");
		Gui->BeginDisabled(ParseSceneRenderPipeline(Candidate.Pipeline) != ESceneRenderPipeline::Deferred);
		if (Gui->Combo("##GBuffer", FormatLabels, FormatIndex,
		               [&](std::size_t InIndex, FVec4 InBounds)
		               {
			               InspectionBounds["render/gbuffer/" + std::string(Formats[InIndex].Token)] = InBounds;
		               }))
		{
			Candidate.GBuffer = ToGBufferPresetToken(RasterOptionIdentity(Formats, FormatIndex));
			bChanged = true;
		}
		InspectionBounds["render/gbuffer"] = Gui->LastItemBounds();
		Gui->EndDisabled();
		Gui->EndPropertyRow();
		bChanged |= Gui->Checkbox("Clustered lighting", Candidate.bClusteredLighting);
		bChanged |= Gui->Checkbox("VSync", Candidate.bVsync);
		InspectionBounds["render/vsync"] = Gui->LastItemBounds();
		bChanged |= Gui->Checkbox("Reversed Z", Candidate.bReversedZ);
		InspectionBounds["render/reversed-z"] = Gui->LastItemBounds();
		Gui->TextWrapped("Changes apply to subsequent frames. Save settings to restore them on the next launch.");
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
