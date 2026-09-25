#include "EditorApplication.h"
#include "Hyperion/Core/Profiling.h"

namespace Hyperion
{
void FEditorPlugin::DrawProfilingCollection()
{
	const auto Profiling = GetProfilingStatus();
	Gui->Separator();
	Gui->Text(Profiling.bCompiled ? (Profiling.bConnected ? "Tracy connected" : "Tracy disconnected")
	                              : "Tracy not compiled");
	Gui->BeginDisabled(!Profiling.bCompiled);
	try
	{
		auto Mask = Profiling.Mask;
		bool bProfile = (Profiling.Mask & ProfileBasicMask) != 0;
		bool bDetail = (Profiling.Mask & ProfileCategoryMask(EProfileCategory::Detail)) != 0;
		bool bGpu = (Profiling.Mask & ProfileCategoryMask(EProfileCategory::Gpu)) != 0;
		if (Gui->Checkbox("Collect CPU scopes", bProfile))
		{
			Mask = bProfile ? Mask | ProfileBasicMask : Mask & ~ProfileBasicMask;
		}
		if (Gui->Checkbox("Collect detail scopes", bDetail))
		{
			const auto Bit = ProfileCategoryMask(EProfileCategory::Detail);
			Mask = bDetail ? Mask | Bit : Mask & ~Bit;
		}
		if (Gui->Checkbox("Collect GPU timings", bGpu))
		{
			const auto Bit = ProfileCategoryMask(EProfileCategory::Gpu);
			Mask = bGpu ? Mask | Bit : Mask & ~Bit;
		}
		InspectionBounds["hud/collect-gpu"] = Gui->LastItemBounds();
		if (Mask != Profiling.Mask)
		{
			ChangeProfiling(Mask, {});
			ProfilingError.clear();
		}
		bool bSampling = Profiling.Sampling == EProfileSampling::Requested;
		if (Gui->Checkbox("Request system sampling", bSampling))
		{
			ChangeProfiling({}, bSampling);
			ProfilingError.clear();
		}
	}
	catch (const std::exception& Failure)
	{
		ProfilingError = Failure.what();
	}
	Gui->EndDisabled();
	if (Profiling.Sampling == EProfileSampling::Unavailable)
	{
		Gui->TextWrapped("System sampling is unavailable; check platform support and ETW access.");
	}
	else if (Profiling.Sampling == EProfileSampling::Requested)
	{
		Gui->TextWrapped("System sampling requested; verify samples in Tracy.");
	}
	Gui->TextWrapped("HUD visibility does not change profiling collection.");
	if (!ProfilingError.empty())
	{
		Gui->TextWrapped(ProfilingError);
	}
}

void FEditorPlugin::DrawProfilingOptions()
{
	if (!Gui->BeginPopup("ProfilingOptions"))
	{
		return;
	}
	Gui->Text("Profiling HUD categories");
	const std::array<const char*, 8> Labels{"Overview",     "Tasks",          "GPU passes", "Device counters",
	                                        "Render views", "Lighting / HZB", "Visibility", "Batching"};
	auto ViewOptions = ViewportState().Options;
	bool bChanged = Gui->Checkbox("Show profiling HUD", *ViewOptions.ProfilingHud);
	for (std::uint32_t Index = 0; Index < Labels.size(); ++Index)
	{
		const auto Bit = 1u << Index;
		bool bSelected = (*ViewOptions.ProfilingCategories & Bit) != 0;
		if (Gui->Checkbox(Labels[Index], bSelected))
		{
			ViewOptions.ProfilingCategories =
			    bSelected ? (*ViewOptions.ProfilingCategories | Bit) : (*ViewOptions.ProfilingCategories & ~Bit);
			bChanged = true;
		}
		InspectionBounds["hud/category/" + std::to_string(Index)] = Gui->LastItemBounds();
	}
	if (bChanged)
	{
		SetViewportOptions(ViewOptions);
	}
	DrawProfilingCollection();
	Gui->EndPopup();
}
} // namespace Hyperion
