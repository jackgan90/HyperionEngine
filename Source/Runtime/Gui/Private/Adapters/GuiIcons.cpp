#include "GuiInternal.h"
#include <algorithm>
#include <cmath>
#include <imgui_internal.h>
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
		case EGuiIcon::Information:
			InDraw.AddCircle(Point(12, 12), 8 * InScale, InColor, 24, 1.5f * InScale);
			Line(12, 11, 12, 17);
			InDraw.AddCircleFilled(Point(12, 7), InScale, InColor);
			break;
		case EGuiIcon::Statistics:
			Line(4, 4, 4, 20);
			Line(4, 20, 21, 20);
			Line(7, 16, 11, 10);
			Line(11, 10, 15, 13);
			Line(15, 13, 20, 5);
			break;
		case EGuiIcon::Capture:
			InDraw.AddRect(Point(3, 7), Point(21, 20), InColor, 2 * InScale, ImDrawFlags_None, 1.5f * InScale);
			InDraw.AddCircle(Point(12, 13), 4 * InScale, InColor, 20, 1.5f * InScale);
			Line(7, 7, 9, 4);
			Line(9, 4, 15, 4);
			Line(15, 4, 17, 7);
			break;
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

void FGui::Tooltip(const char* InText, std::span<const FPropertyTooltipLine> InLines)
{
	Impl->Select();
	if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
	{
		return;
	}
	if (InLines.empty())
	{
		ImGui::SetTooltip("%s", Impl->PathDisplay.Text(InText ? InText : "").c_str());
		return;
	}
	if (ImGui::BeginTooltip())
	{
		if (InText && *InText)
		{
			ImGui::TextUnformatted(Impl->PathDisplay.Text(InText).c_str());
		}
		for (const auto& Line : InLines)
		{
			const auto Color = Line.Tone == EPropertyTooltipTone::Positive   ? ImVec4{.4f, .85f, .4f, 1}
			                   : Line.Tone == EPropertyTooltipTone::Negative ? ImVec4{1, .35f, .35f, 1}
			                                                                 : ImGui::GetStyleColorVec4(ImGuiCol_Text);
			ImGui::PushStyleColor(ImGuiCol_Text, Color);
			ImGui::TextUnformatted(Impl->PathDisplay.Text(Line.Text).c_str());
			ImGui::PopStyleColor();
		}
		ImGui::EndTooltip();
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

bool FGui::BeginPopup(const char* InId, FVec4 InAnchor)
{
	Impl->Select();
	const auto* Viewport = ImGui::GetMainViewport();
	const float Padding = ImGui::GetStyle().DisplaySafeAreaPadding.x;
	const float Left = Viewport->WorkPos.x + Padding;
	const float Right = Viewport->WorkPos.x + Viewport->WorkSize.x - Padding;
	const float MaxWidth = std::max(1.f, Right - Left);
	const float MaxHeight = std::max(ImGui::GetFrameHeight(), Viewport->WorkPos.y + Viewport->WorkSize.y - InAnchor.W -
	                                                              ImGui::GetStyle().DisplaySafeAreaPadding.y);
	ImGui::SetNextWindowSizeConstraints({std::min(InAnchor.Z - InAnchor.X, MaxWidth), 0}, {MaxWidth, MaxHeight});
	float Width = std::min(InAnchor.Z - InAnchor.X, MaxWidth);
	if (ImGui::IsPopupOpen(InId))
	{
		const auto* Popup = Impl->Context->OpenPopupStack.Data + Impl->Context->BeginPopupStack.Size;
		if (Popup->Window && Popup->Window->WasActive)
		{
			Width = ImGui::CalcWindowNextAutoFitSize(Popup->Window).x;
		}
	}
	// Resolve placement before Begin creates the background and clip rectangle, as combo popups do.
	ImGui::SetNextWindowPos({std::clamp(InAnchor.X, Left, std::max(Left, Right - Width)), InAnchor.W},
	                        ImGuiCond_Always);
	return ImGui::BeginPopup(InId, ImGuiWindowFlags_NoMove);
}

void FGui::EndPopup()
{
	Impl->Select();
	ImGui::EndPopup();
}
} // namespace Hyperion
