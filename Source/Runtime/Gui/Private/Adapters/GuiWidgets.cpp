#include "GuiInternal.h"
#include <algorithm>

namespace Hyperion
{
bool FGui::InputNumber(const char* InLabel, double& InValue)
{
	Impl->Select();
	return ImGui::InputScalar(InLabel, ImGuiDataType_Double, &InValue, nullptr, nullptr, "%.9g",
	                          ImGuiInputTextFlags_EnterReturnsTrue);
}

bool FGui::InputInteger(const char* InLabel, std::int64_t& InValue)
{
	Impl->Select();
	return ImGui::InputScalar(InLabel, ImGuiDataType_S64, &InValue, nullptr, nullptr, nullptr,
	                          ImGuiInputTextFlags_EnterReturnsTrue);
}

bool FGui::InputInteger(const char* InLabel, std::uint64_t& InValue)
{
	Impl->Select();
	return ImGui::InputScalar(InLabel, ImGuiDataType_U64, &InValue, nullptr, nullptr, nullptr,
	                          ImGuiInputTextFlags_EnterReturnsTrue);
}

void FGui::BeginDisabled(bool bInDisabled)
{
	Impl->Select();
	ImGui::BeginDisabled(bInDisabled);
}

void FGui::EndDisabled()
{
	Impl->Select();
	ImGui::EndDisabled();
}

bool FGui::BeginMenuBar()
{
	Impl->Select();
	return ImGui::BeginMainMenuBar();
}

void FGui::EndMenuBar()
{
	Impl->Select();
	ImGui::EndMainMenuBar();
}

bool FGui::BeginMenu(const char* InLabel)
{
	Impl->Select();
	return ImGui::BeginMenu(InLabel);
}

void FGui::EndMenu()
{
	Impl->Select();
	ImGui::EndMenu();
}

bool FGui::MenuItem(const char* InLabel, const char* InShortcut, bool bInSelected)
{
	Impl->Select();
	return ImGui::MenuItem(InLabel, InShortcut, bInSelected);
}

void FGui::SameLine()
{
	Impl->Select();
	ImGui::SameLine();
}

void FGui::SetNextItemWidth(float InWidth)
{
	Impl->Select();
	ImGui::SetNextItemWidth(InWidth);
}

bool FGui::Section(const char* InLabel, bool bInDefaultOpen)
{
	Impl->Select();
	ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_TableHeaderBg));
	const bool bOpen = ImGui::CollapsingHeader(InLabel, bInDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
	ImGui::PopStyleColor();
	return bOpen;
}

bool FGui::BeginTable(const char* InId, const char* InFirst, const char* InSecond)
{
	Impl->Select();
	if (!ImGui::BeginTable(InId, 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
	{
		return false;
	}
	ImGui::TableSetupColumn(InFirst, ImGuiTableColumnFlags_WidthStretch, 2);
	ImGui::TableSetupColumn(InSecond, ImGuiTableColumnFlags_WidthStretch, 1);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();
	return true;
}

void FGui::NextRow()
{
	Impl->Select();
	ImGui::TableNextRow();
}

void FGui::NextColumn()
{
	Impl->Select();
	ImGui::TableNextColumn();
}

void FGui::EndTable()
{
	Impl->Select();
	ImGui::EndTable();
}

bool FGui::TreeItem(const char* InId, const char* InLabel, bool bInLeaf, bool bInSelected, bool& bOutClicked,
                    bool bInDefaultOpen)
{
	Impl->Select();
	const auto Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAllColumns |
	                   (bInDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0) | (bInLeaf ? ImGuiTreeNodeFlags_Leaf : 0) |
	                   (bInSelected ? ImGuiTreeNodeFlags_Selected : 0);
	const bool bOpen = ImGui::TreeNodeEx(InId, Flags, "%s", InLabel);
	bOutClicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
	return bOpen;
}

void FGui::EndTree()
{
	Impl->Select();
	ImGui::TreePop();
}

void FGui::Property(const char* InLabel, const std::string& InValue)
{
	Impl->Select();
	ImGui::TextDisabled("%s", InLabel);
	ImGui::SameLine(std::max(90.f, ImGui::GetContentRegionAvail().x * .38f));
	ImGui::TextUnformatted(InValue.c_str());
}

FGuiImageRegion FGui::Image(std::uint64_t InTextureId)
{
	Impl->Select();
	const auto Available = ImGui::GetContentRegionAvail();
	ImGui::Image(ImTextureID(InTextureId), {std::max(1.f, Available.x), std::max(1.f, Available.y)});
	const auto Minimum = ImGui::GetItemRectMin();
	const auto Maximum = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	if (bHovered && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
	{
		ImGui::SetWindowFocus();
	}
	const bool bFocused = ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() &&
	                      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	ImGui::GetWindowDrawList()->AddRect(Minimum, Maximum,
	                                    bFocused ? IM_COL32(206, 153, 50, 255) : IM_COL32(45, 45, 45, 255));
	return {{Minimum.x, Minimum.y, Maximum.x, Maximum.y}, bHovered, bFocused};
}

void FGui::OpenPopup(const char* InTitle)
{
	Impl->Select();
	ImGui::OpenPopup(InTitle);
}

bool FGui::BeginModal(const char* InTitle, bool& bInOpen)
{
	Impl->Select();
	ImGui::SetNextWindowSize({660, 460}, ImGuiCond_FirstUseEver);
	const auto* Viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(Viewport->GetCenter(), ImGuiCond_Appearing, {.5f, .5f});
	return ImGui::BeginPopupModal(InTitle, &bInOpen, ImGuiWindowFlags_NoCollapse);
}

void FGui::EndModal()
{
	Impl->Select();
	ImGui::EndPopup();
}

void FGui::ClosePopup()
{
	Impl->Select();
	ImGui::CloseCurrentPopup();
}

bool FGui::IsEditingText() const
{
	Impl->Select();
	return ImGui::GetIO().WantTextInput;
}
} // namespace Hyperion
