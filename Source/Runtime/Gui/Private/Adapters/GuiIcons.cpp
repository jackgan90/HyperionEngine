#include "GuiInternal.h"
#include <cmath>
#include <numbers>

namespace Hyperion
{
namespace
{
// All icons share a 24-unit canvas and the editor theme's foreground color.
void DrawIcon(ImDrawList& InDraw, EGuiIcon InIcon, ImVec2 InOrigin, float InScale, ImU32 InColor)
{
	const auto Point = [&](float InX, float InY)
	{
		return ImVec2{InOrigin.x + InX * InScale, InOrigin.y + InY * InScale};
	};
	const auto Line = [&](float InX, float InY, float InEndX, float InEndY)
	{
		InDraw.AddLine(Point(InX, InY), Point(InEndX, InEndY), InColor, 1.5f * InScale);
	};
	const auto Arrow = [&](float InX, float InY, float InDx, float InDy)
	{
		const float Length = std::hypot(InDx, InDy);
		const float X = InDx / Length;
		const float Y = InDy / Length;
		Line(InX, InY, InX - X * 3 - Y * 3, InY - Y * 3 + X * 3);
		Line(InX, InY, InX - X * 3 + Y * 3, InY - Y * 3 - X * 3);
	};
	switch (InIcon)
	{
		case EGuiIcon::Translate:
			Line(4, 12, 20, 12);
			Line(12, 4, 12, 20);
			Arrow(4, 12, -1, 0);
			Arrow(20, 12, 1, 0);
			Arrow(12, 4, 0, -1);
			Arrow(12, 20, 0, 1);
			break;
		case EGuiIcon::Rotate:
		{
			constexpr float Pi = std::numbers::pi_v<float>;
			for (unsigned Index = 0; Index <= 32; ++Index)
			{
				const float Angle = (-35 + Index * (290.f / 32)) * Pi / 180;
				InDraw.PathLineTo(Point(12 + 7 * std::cos(Angle), 12 + 7 * std::sin(Angle)));
			}
			InDraw.PathStroke(InColor, ImDrawFlags_None, 1.5f * InScale);
			const float End = 255 * Pi / 180;
			Arrow(12 + 7 * std::cos(End), 12 + 7 * std::sin(End), -std::sin(End), std::cos(End));
			break;
		}
		case EGuiIcon::Scale:
			InDraw.AddRect(Point(4, 13), Point(11, 20), InColor, 0, ImDrawFlags_None, 1.5f * InScale);
			Line(9, 15, 19, 5);
			Line(13, 5, 19, 5);
			Line(19, 5, 19, 11);
			break;
		case EGuiIcon::Options:
			for (float Y : {6.f, 12.f, 18.f})
			{
				const float X = Y == 12 ? 15.f : 9.f;
				Line(4, Y, X - 2, Y);
				Line(X + 2, Y, 20, Y);
				InDraw.AddCircle(Point(X, Y), 2 * InScale, InColor, 12, 1.5f * InScale);
			}
			break;
	}
}
} // namespace

bool FGui::IconButton(const char* InId, EGuiIcon InIcon, const char* InTooltip, bool bInSelected)
{
	Impl->Select();
	const float Size = ImGui::GetFrameHeight();
	const auto Origin = ImGui::GetCursorScreenPos();
	ImGui::PushStyleColor(ImGuiCol_Button, bInSelected ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4{});
	const bool bPressed = ImGui::Button(InId, {Size, Size});
	ImGui::PopStyleColor();
	DrawIcon(*ImGui::GetWindowDrawList(), InIcon, Origin, Size / 24, ImGui::GetColorU32(ImGuiCol_Text));
	Tooltip(InTooltip);
	return Impl->TrackEdit(bPressed);
}

void FGui::Tooltip(const char* InText)
{
	Impl->Select();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("%s", InText);
	}
}

float FGui::AvailableWidth() const
{
	Impl->Select();
	return ImGui::GetContentRegionAvail().x / Impl->AppliedScale;
}

bool FGui::BeginPopup(const char* InId)
{
	Impl->Select();
	return ImGui::BeginPopup(InId);
}

void FGui::EndPopup()
{
	Impl->Select();
	ImGui::EndPopup();
}
} // namespace Hyperion
