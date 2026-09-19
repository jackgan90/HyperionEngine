#include "GuiInternal.h"
#include <algorithm>
#include <imgui_internal.h>

namespace Hyperion
{
bool FGui::InputNumber(const char* InLabel, double& InValue)
{
	Impl->Select();
	return Impl->DragNumber(InLabel, ImGuiDataType_Double, &InValue, .01f, "%.9g");
}

bool FGui::InputInteger(const char* InLabel, std::int64_t& InValue)
{
	Impl->Select();
	return Impl->DragNumber(InLabel, ImGuiDataType_S64, &InValue, 1);
}

bool FGui::InputInteger(const char* InLabel, std::uint64_t& InValue)
{
	Impl->Select();
	return Impl->DragNumber(InLabel, ImGuiDataType_U64, &InValue, 1);
}

void FGui::BeginPropertyRow(const char* InLabel, bool* bOutExpanded)
{
	Impl->Select();
	ImGui::PushID(InLabel);
	const float Start = ImGui::GetCursorPosX();
	const float Available = ImGui::GetContentRegionAvail().x;
	const std::string_view Label(InLabel);
	const auto Marker = Label.find("##");
	const char* End = InLabel + (Marker == std::string_view::npos ? Label.size() : Marker);
	const float LabelWidth = std::max(ImGui::CalcTextSize(InLabel, End).x + ImGui::GetStyle().ItemInnerSpacing.x,
	                                  std::clamp(Available * .38f, Scale(76), Scale(160)));
	ImGui::AlignTextToFramePadding();
	if (bOutExpanded)
	{
		*bOutExpanded =
		    ImGui::TreeNodeEx("##expand", ImGuiTreeNodeFlags_NoTreePushOnOpen, "%.*s", int(End - InLabel), InLabel);
	}
	else
	{
		ImGui::TextUnformatted(InLabel, End);
	}
	ImGui::SameLine(Start + LabelWidth);
	ImGui::SetNextItemWidth(std::max(1.f, ImGui::GetContentRegionAvail().x));
}

void FGui::EndPropertyRow()
{
	Impl->Select();
	ImGui::PopID();
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
	ImGui::SetNextItemWidth(InWidth > 0 ? Scale(InWidth) : InWidth);
}

void FGui::SameLineIfFits(const char* InButtonLabel)
{
	Impl->Select();
	const auto& Style = ImGui::GetStyle();
	const float Right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
	const float Width = ImGui::CalcTextSize(InButtonLabel, nullptr, true).x + Style.FramePadding.x * 2;
	if (ImGui::GetItemRectMax().x + Style.ItemSpacing.x + Width <= Right)
	{
		ImGui::SameLine();
	}
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
	ImGui::SameLine(std::max(Scale(90), ImGui::GetContentRegionAvail().x * .38f));
	ImGui::TextUnformatted(InValue.c_str());
}

FGuiImageRegion FGui::Image(std::uint64_t InTextureId)
{
	Impl->Select();
	const auto CaptureId = ImGui::GetID("##ImageOverlayPointer");
	if (Impl->ImagePointerCapture == CaptureId)
	{
		ImGui::KeepAliveID(CaptureId);
	}
	const auto Available = ImGui::GetContentRegionAvail();
	ImGui::Image(ImTextureID(InTextureId), {std::max(1.f, Available.x), std::max(1.f, Available.y)});
	const auto Minimum = ImGui::GetItemRectMin();
	const auto Maximum = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	if (bHovered && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
	{
		ImGui::SetWindowFocus();
	}
	const bool bFocused =
	    ImGui::IsWindowFocused() &&
	    (!ImGui::IsAnyItemActive() || (Impl->ImagePointerCapture == CaptureId && ImGui::GetActiveID() == CaptureId)) &&
	    !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	ImGui::GetWindowDrawList()->AddRect(Minimum, Maximum,
	                                    bFocused ? IM_COL32(206, 153, 50, 255) : IM_COL32(45, 45, 45, 255));
	return {{Minimum.x, Minimum.y, Maximum.x, Maximum.y}, bHovered, bFocused};
}

FGuiPointerState FGui::PointerState() const
{
	Impl->Select();
	const auto Position = ImGui::GetMousePos();
	return {{Position.x, Position.y},
	        ImGui::IsMouseClicked(0),
	        ImGui::IsMouseDown(0),
	        ImGui::IsMouseReleased(0),
	        ImGui::IsKeyPressed(ImGuiKey_Escape),
	        ImGui::IsMouseDown(1),
	        ImGui::IsMousePosValid(&Position)};
}

void FGui::CaptureImagePointer(bool bInCapture)
{
	Impl->Select();
	if (bInCapture)
	{
		Impl->ImagePointerCapture = ImGui::GetID("##ImageOverlayPointer");
		ImGui::SetActiveID(Impl->ImagePointerCapture, ImGui::GetCurrentWindow());
		ImGui::KeepAliveID(Impl->ImagePointerCapture);
	}
	else
	{
		if (Impl->ImagePointerCapture && ImGui::GetActiveID() == Impl->ImagePointerCapture)
		{
			ImGui::ClearActiveID();
		}
		Impl->ImagePointerCapture = 0;
	}
}

void FGui::DrawImageOverlay(FVec4 InClip, std::span<const FVec2> InPoints, FVec4 InColor, float InThickness,
                            bool bInFilled)
{
	Impl->Select();
	if (InPoints.size() < 2)
	{
		return;
	}
	std::vector<ImVec2> Points;
	Points.reserve(InPoints.size());
	for (const auto Point : InPoints)
	{
		Points.emplace_back(Point.X, Point.Y);
	}
	auto* Draw = ImGui::GetWindowDrawList();
	Draw->PushClipRect({InClip.X, InClip.Y}, {InClip.Z, InClip.W}, true);
	const auto Color = ImGui::ColorConvertFloat4ToU32({InColor.X, InColor.Y, InColor.Z, InColor.W});
	if (bInFilled && Points.size() >= 3)
	{
		Draw->AddConvexPolyFilled(Points.data(), static_cast<int>(Points.size()), Color);
	}
	else
	{
		Draw->AddPolyline(Points.data(), static_cast<int>(Points.size()), Color, ImDrawFlags_None, InThickness);
	}
	Draw->PopClipRect();
}

void FGui::OpenPopup(const char* InTitle)
{
	Impl->Select();
	ImGui::OpenPopup(InTitle);
}

bool FGui::BeginModal(const char* InTitle, bool& bInOpen)
{
	Impl->Select();
	ImGui::SetNextWindowSize({Scale(660), Scale(460)}, ImGuiCond_FirstUseEver);
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
