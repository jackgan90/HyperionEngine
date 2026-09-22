#include "GuiInternal.h"
#include <imgui_internal.h>

namespace Hyperion
{
bool FGui::BeginTabBar(const char* InId)
{
	Impl->Select();
	return ImGui::BeginTabBar(InId, ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs);
}

void FGui::EndTabBar()
{
	Impl->Select();
	ImGui::EndTabBar();
}

bool FGui::BeginTabItem(const char* InLabel, bool* bInOpen, bool bInActivate)
{
	Impl->Select();
	// The owner decides whether a close request is accepted, for example after an unsaved-document prompt.
	const auto Flags =
	    ImGuiTabItemFlags_NoAssumedClosure | (bInActivate ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None);
	return ImGui::BeginTabItem(InLabel, bInOpen, Flags);
}

void FGui::EndTabItem()
{
	Impl->Select();
	ImGui::EndTabItem();
}

void FGui::UseEditorStyle()
{
	Impl->Select();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetStyle() = Impl->BaseStyle;
	ImGui::StyleColorsDark();
	auto& Style = ImGui::GetStyle();
	Style.WindowRounding = 0;
	Style.ChildRounding = 0;
	Style.FrameRounding = 2;
	Style.PopupRounding = 3;
	Style.TabRounding = 2;
	Style.GrabRounding = 2;
	Style.WindowPadding = {8, 8};
	Style.FramePadding = {7, 4};
	Style.ItemSpacing = {7, 5};
	Style.IndentSpacing = 14;
	Style.ScrollbarSize = 12;
	Style.WindowBorderSize = 1;
	Style.FrameBorderSize = 0;
	Style.DockingSeparatorSize = 3;
	auto* Colors = Style.Colors;
	Colors[ImGuiCol_Text] = {.82f, .82f, .82f, 1};
	Colors[ImGuiCol_TextDisabled] = {.47f, .47f, .47f, 1};
	Colors[ImGuiCol_WindowBg] = {.115f, .115f, .115f, 1};
	Colors[ImGuiCol_ChildBg] = {.10f, .10f, .10f, 1};
	Colors[ImGuiCol_PopupBg] = {.13f, .13f, .13f, 1};
	Colors[ImGuiCol_Border] = {.065f, .065f, .065f, 1};
	Colors[ImGuiCol_FrameBg] = {.075f, .075f, .075f, 1};
	Colors[ImGuiCol_FrameBgHovered] = {.21f, .21f, .21f, 1};
	Colors[ImGuiCol_FrameBgActive] = {.25f, .25f, .25f, 1};
	Colors[ImGuiCol_TitleBg] = {.075f, .075f, .075f, 1};
	Colors[ImGuiCol_TitleBgActive] = {.15f, .15f, .15f, 1};
	Colors[ImGuiCol_MenuBarBg] = {.085f, .085f, .085f, 1};
	Colors[ImGuiCol_Header] = {.02f, .34f, .62f, 1};
	Colors[ImGuiCol_HeaderHovered] = {.18f, .29f, .40f, 1};
	Colors[ImGuiCol_HeaderActive] = {.02f, .36f, .66f, 1};
	Colors[ImGuiCol_Button] = {.19f, .19f, .19f, 1};
	Colors[ImGuiCol_ButtonHovered] = {.27f, .27f, .27f, 1};
	Colors[ImGuiCol_ButtonActive] = {.04f, .36f, .65f, 1};
	Colors[ImGuiCol_Tab] = {.085f, .085f, .085f, 1};
	Colors[ImGuiCol_TabSelected] = {.17f, .17f, .17f, 1};
	Colors[ImGuiCol_TabDimmed] = {.085f, .085f, .085f, 1};
	Colors[ImGuiCol_TabDimmedSelected] = {.145f, .145f, .145f, 1};
	Colors[ImGuiCol_TabHovered] = {.23f, .23f, .23f, 1};
	Colors[ImGuiCol_TabSelectedOverline] = {.08f, .43f, .78f, 1};
	Colors[ImGuiCol_CheckMark] = {.15f, .55f, .90f, 1};
	Colors[ImGuiCol_SliderGrab] = {.15f, .55f, .90f, 1};
	Colors[ImGuiCol_Separator] = {.065f, .065f, .065f, 1};
	Colors[ImGuiCol_DockingPreview] = {.10f, .45f, .80f, .6f};
	Colors[ImGuiCol_DockingEmptyBg] = {.065f, .065f, .065f, 1};
	Colors[ImGuiCol_TableHeaderBg] = {.16f, .16f, .16f, 1};
	Colors[ImGuiCol_TableRowBgAlt] = {1, 1, 1, .025f};
	Impl->BaseStyle = Style;
	Style.ScaleAllSizes(Impl->AppliedScale);
	Style.FontScaleMain = 1 / Impl->FontDensity;
}

void FGui::LoadLayout(std::string_view InLayout)
{
	Impl->Select();
	ImGui::LoadIniSettingsFromMemory(InLayout.data(), InLayout.size());
}

std::string FGui::SaveLayout()
{
	Impl->Select();
	size_t Size{};
	const char* Data = ImGui::SaveIniSettingsToMemory(&Size);
	return {Data, Size};
}

void FGui::DockSpace(const FGuiDockLayout& InLayout, bool bInReset)
{
	Impl->Select();
	const auto Id = ImGui::GetID("HyperionWorkspace");
	auto* Viewport = static_cast<ImGuiViewportP*>(ImGui::GetMainViewport());
	// Side bars reserve their current-frame space in BuildWorkInset. ImGui's
	// public WorkPos/WorkSize otherwise lag a frame behind runtime scale changes.
	const auto WorkRect = Viewport->GetBuildWorkRect();
	Viewport->WorkPos = WorkRect.Min;
	Viewport->WorkSize = WorkRect.GetSize();
	if (bInReset || !ImGui::DockBuilderGetNode(Id))
	{
		ImGui::DockBuilderRemoveNode(Id);
		ImGui::DockBuilderAddNode(Id, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(Id, Viewport->WorkSize);
		auto Center = Id;
		if (!InLayout.Left.empty())
		{
			const auto Left = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Left, .20f, nullptr, &Center);
			ImGui::DockBuilderDockWindow(InLayout.Left.c_str(), Left);
		}
		if (!InLayout.RightTop.empty() || !InLayout.RightBottom.empty())
		{
			auto Right = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Right, InLayout.RightFraction, nullptr, &Center);
			if (!InLayout.RightTop.empty() && !InLayout.RightBottom.empty())
			{
				const auto Top = ImGui::DockBuilderSplitNode(Right, ImGuiDir_Up, .43f, nullptr, &Right);
				ImGui::DockBuilderDockWindow(InLayout.RightTop.c_str(), Top);
			}
			const auto& RightName = InLayout.RightBottom.empty() ? InLayout.RightTop : InLayout.RightBottom;
			ImGui::DockBuilderDockWindow(RightName.c_str(), Right);
		}
		if (!InLayout.Bottom.empty())
		{
			const auto Bottom = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Down, .30f, nullptr, &Center);
			ImGui::DockBuilderDockWindow(InLayout.Bottom.c_str(), Bottom);
		}
		ImGui::DockBuilderDockWindow(InLayout.Center.c_str(), Center);
		ImGui::DockBuilderFinish(Id);
	}
	ImGui::DockSpaceOverViewport(Id, Viewport);
}

bool FGui::BeginWindow(const char* InTitle, bool& bInOpen)
{
	Impl->Select();
	return ImGui::Begin(InTitle, &bInOpen, ImGuiWindowFlags_NoCollapse);
}

void FGui::EndWindow()
{
	Impl->Select();
	ImGui::End();
}

bool FGui::BeginToolbar()
{
	Impl->Select();
	return ImGui::BeginViewportSideBar("##Toolbar", ImGui::GetMainViewport(), ImGuiDir_Up, Scale(38),
	                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
}

void FGui::EndToolbar()
{
	EndWindow();
}

void FGui::StatusBar(const std::string& InText)
{
	Impl->Select();
	if (ImGui::BeginViewportSideBar("##Status", ImGui::GetMainViewport(), ImGuiDir_Down, Scale(26),
	                                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
	{
		Text(InText);
	}
	ImGui::End();
}
} // namespace Hyperion
