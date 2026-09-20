#pragma once
#include "Hyperion/Gui/Gui.h"
#include <imgui.h>
#include <implot.h>
#include <thread>

namespace Hyperion
{
bool DragNumericValue(const char* InLabel, ImGuiDataType InType, void* InValue, float InSpeed,
                      const char* InFormat = nullptr, const void* InMinimum = nullptr, const void* InMaximum = nullptr);

struct FGui::FImpl
{
	ImGuiContext* Context{};
	ImPlotContext* Plot{};
	std::thread::id Thread = std::this_thread::get_id();
	FWindow* Window{};
	std::string Clipboard;
	std::vector<std::byte> FontBytes;
	ImGuiStyle BaseStyle;
	ImPlotStyle BasePlotStyle;
	float RequestedScale = 1;
	float AppliedScale = 1;
	float FontPixels = 13;
	float FontDensity = 1;
	std::shared_ptr<const FImage> FontAtlas;
	void ApplyScale(float InDensity);
	void RebuildFont();
	bool bLiveEdit{};
	FGuiEditState EditState;
	ImGuiID ImagePointerCapture{};
	bool bDragCancelled{};
	ImGuiID EditItem{};
	std::uint64_t EditSerial{};
	int EditFrame = -2;

	ImGuiInputTextFlags InputFlags() const
	{
		return bLiveEdit ? ImGuiInputTextFlags_NoUndoRedo : ImGuiInputTextFlags_EnterReturnsTrue;
	}

	bool DragNumber(const char* InLabel, ImGuiDataType InType, void* InValue, float InSpeed,
	                const char* InFormat = nullptr, const void* InMinimum = nullptr, const void* InMaximum = nullptr,
	                bool bInMixed = false);
	bool DragComponents(const char* InLabel, float* InValues, int InCount);
	bool TrackEdit(bool bInChanged);
	bool TrackEdit(ImGuiID InId, bool bInActive, bool bInActivated, bool bInChanged);

	void Select()
	{
		if (Thread != std::this_thread::get_id())
		{
			throw std::logic_error("GUI called from non-owning thread");
		}
		ImGui::SetCurrentContext(Context);
		ImPlot::SetCurrentContext(Plot);
	}

	~FImpl()
	{
		if (Plot)
		{
			ImPlot::DestroyContext(Plot);
		}
		if (Context)
		{
			ImGui::DestroyContext(Context);
		}
	}
};
} // namespace Hyperion
