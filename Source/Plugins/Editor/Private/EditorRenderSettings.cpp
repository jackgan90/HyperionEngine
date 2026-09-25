#include "EditorApplication.h"
#include "Hyperion/Core/Profiling.h"

namespace Hyperion
{
FRenderSettingsState FEditorPlugin::RenderSettings() const
{
	return {Rendering, RenderSettingsRevision, Options.Rendering.bReversedZ};
}

void FEditorPlugin::SetRenderSettings(std::uint64_t InRevision, const FRenderSettings& InSettings)
{
	if (InRevision != RenderSettingsRevision)
	{
		throw FSceneEditError("stale_revision", "Render settings changed; query the current values");
	}
	ValidateRenderSettings(InSettings);
	if (InSettings.Contact != Rendering.Contact && (InSettings.Contact.bEnabled || InSettings.Contact.DebugMode) &&
	    !Context.Require<FRenderFeatureRegistry>().Contains("contact-shadows"))
	{
		throw FSceneEditError("unavailable", "Contact shadow feature is disabled in this session");
	}
	MakePipelineSettings(InSettings).GBuffer.Validate(Device->GetCapabilities());
	Rendering = InSettings;
	Exposure = Rendering.Exposure;
	++RenderSettingsRevision;
}

void FEditorPlugin::SaveRenderSettings(const std::filesystem::path& InPath)
{
	const auto Path = InPath.empty() ? Options.RenderSettingsPath : InPath;
	if (Path.empty())
	{
		throw std::invalid_argument("Provide a target-local settings path for this isolated session");
	}
	WriteRenderSettings(Path, Rendering);
}

FCascadedShadowSettings FEditorPlugin::ShadowControls() const
{
	return Rendering.Shadows;
}

void FEditorPlugin::SetShadowControls(const FCascadedShadowSettings& InSettings)
{
	auto Candidate = Rendering;
	Candidate.Shadows = InSettings;
	SetRenderSettings(RenderSettingsRevision, Candidate);
}

void FEditorPlugin::ChangeProfiling(std::optional<std::uint32_t> InMask, std::optional<bool> InSampling)
{
	if (!GetProfilingStatus().bCompiled)
	{
		throw FSceneEditError("unavailable", "Profiling is not compiled in this build");
	}
	if (InMask && (*InMask & ~ProfileAllMask))
	{
		throw std::invalid_argument("Unknown profiling category bits");
	}
	// Editor joins its Render/RHI work before each Main update, so no frame can retain the old mask.
	if (InMask)
	{
		SetProfilingMask(*InMask);
	}
	if (InSampling)
	{
		SetProfilingSampling(*InSampling);
	}
}
} // namespace Hyperion
