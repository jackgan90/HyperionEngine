#include "GuiInternal.h"
#include <algorithm>
#include <cstring>
#include <imgui_internal.h>

namespace Hyperion
{
void FGui::RevealLastItem()
{
	Impl->Select();
	ImGui::SetScrollHereY(.5f);
}

namespace
{
void DrawCenteredTileLabel(ImDrawList& InDraw, const char* InLabel, ImVec2 InMinimum, ImVec2 InMaximum)
{
	auto* Font = ImGui::GetFont();
	const float FontSize = ImGui::GetFontSize();
	const float Width = InMaximum.x - InMinimum.x;
	const auto* End = InLabel + std::strlen(InLabel);
	const auto* Line = InLabel;
	float Y = InMinimum.y;
	while (Line < End && Y < InMaximum.y)
	{
		const auto* LineEnd = Font->CalcWordWrapPosition(FontSize, Line, End, Width);
		const float LineWidth = ImGui::CalcTextSize(Line, LineEnd, false).x;
		InDraw.AddText(Font, FontSize, {InMinimum.x + (Width - LineWidth) * .5f, Y}, ImGui::GetColorU32(ImGuiCol_Text),
		               Line, LineEnd);
		Line = ImTextCalcWordWrapNextLineStart(LineEnd, End);
		Y += FontSize;
	}
}
} // namespace

void FGui::ClosePopups()
{
	Impl->Select();
	if (!ImGui::GetCurrentContext()->OpenPopupStack.empty())
	{
		ImGui::ClosePopupToLevel(0, true);
	}
}

bool FGui::BeginSplitPane(const char* InId, float InLeftWidth)
{
	Impl->Select();
	if (!ImGui::BeginTable(InId, 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		return false;
	}
	ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, Scale(InLeftWidth));
	ImGui::TableSetupColumn("Contents", ImGuiTableColumnFlags_WidthStretch);
	return true;
}

bool FGui::BeginTileGrid(const char* InId, float InTileWidth)
{
	Impl->Select();
	const int Columns = std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x / Scale(InTileWidth)), 1, 64);
	return ImGui::BeginTable(InId, Columns, ImGuiTableFlags_SizingStretchSame);
}

void FGui::SetNextTreeOpen(bool bInOpen)
{
	Impl->Select();
	ImGui::SetNextItemOpen(bInOpen);
}

bool FGui::FileTile(const char* InId, const char* InLabel, bool bInFolder, bool bInSelected, bool& bOutDoubleClicked)
{
	Impl->Select();
	const auto Start = ImGui::GetCursorScreenPos();
	const ImVec2 Size{std::max(Scale(40), ImGui::GetContentRegionAvail().x), Scale(108)};
	const bool bClicked = ImGui::InvisibleButton(InId, Size);
	const bool bHovered = ImGui::IsItemHovered();
	bOutDoubleClicked = bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
	auto* Draw = ImGui::GetWindowDrawList();
	if (bInSelected || bHovered)
	{
		Draw->AddRectFilled(Start, {Start.x + Size.x, Start.y + Size.y},
		                    ImGui::GetColorU32(bInSelected ? ImGuiCol_Header : ImGuiCol_HeaderHovered), Scale(4));
	}
	const float Width = std::min(Scale(66), Size.x - Scale(12));
	const ImVec2 Origin{Start.x + (Size.x - Width) * .5f, Start.y + Scale(12)};
	if (bInFolder)
	{
		Draw->AddRectFilled(Origin, {Origin.x + Width * .43f, Origin.y + Scale(16)}, IM_COL32(166, 130, 62, 255),
		                    Scale(3));
		Draw->AddRectFilled({Origin.x, Origin.y + Scale(8)}, {Origin.x + Width, Origin.y + Scale(51)},
		                    IM_COL32(193, 153, 78, 255), Scale(3));
	}
	else
	{
		const float Left = Origin.x + Width * .17f;
		const float Right = Origin.x + Width * .83f;
		Draw->AddRectFilled({Left, Origin.y}, {Right, Origin.y + Scale(53)}, IM_COL32(130, 158, 190, 255), Scale(3));
		for (int Index = 0; Index < 3; ++Index)
		{
			const float Y = Origin.y + Scale(22 + float(Index) * 8);
			Draw->AddLine({Left + Scale(8), Y}, {Right - Scale(8), Y}, IM_COL32(52, 69, 87, 255), Scale(2));
		}
	}
	Draw->PushClipRect({Start.x + Scale(4), Start.y + Scale(71)}, {Start.x + Size.x - Scale(4), Start.y + Size.y},
	                   true);
	DrawCenteredTileLabel(*Draw, InLabel, {Start.x + Scale(6), Start.y + Scale(73)},
	                      {Start.x + Size.x - Scale(6), Start.y + Size.y});
	Draw->PopClipRect();
	if (bHovered && !bInFolder)
	{
		ImGui::SetTooltip("%s", InLabel);
	}
	return bClicked || bOutDoubleClicked;
}
} // namespace Hyperion
