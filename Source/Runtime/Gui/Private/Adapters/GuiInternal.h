#pragma once
#include "Hyperion/Gui/Gui.h"
#include <imgui.h>
#include <implot.h>
#include <thread>

namespace Hyperion
{
struct FGui::FImpl
{
	ImGuiContext* Context{};
	ImPlotContext* Plot{};
	std::thread::id Thread = std::this_thread::get_id();
	FWindow* Window{};
	std::string Clipboard;
	std::vector<std::byte> FontBytes;

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
