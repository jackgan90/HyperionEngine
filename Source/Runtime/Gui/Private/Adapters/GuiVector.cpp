#include "GuiInternal.h"
#include <algorithm>

namespace Hyperion
{
bool FGui::InputVectorRow(const char* InLabel, FVec3& InValue, std::string_view InUnit, std::string_view InTooltip,
                          std::array<FVec4, 3>& OutBounds, const std::array<bool, 3>& InMixed,
                          std::array<bool, 3>* OutEdited)
{
	Impl->Select();
	ImGui::BeginGroup();
	BeginPropertyRow(InLabel);
	const float Start = ImGui::GetCursorPosX();
	const float Spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const float Available = ImGui::GetContentRegionAvail().x;
	const float MixedWidth = ImGui::CalcTextSize("Multiple Values").x + ImGui::CalcTextSize("X").x +
	                         ImGui::GetStyle().FramePadding.x * 2 + Spacing * 3;
	// Keep the layout stable as mixed fields become equal during an active selection edit.
	const bool bStacked = OutEdited && Available < MixedWidth * 3;
	const float Width = bStacked ? Available : std::max(Scale(24), (Available - Spacing * 4) / 3);
	std::string Format = "%.6g";
	if (!InUnit.empty())
	{
		Format += " ";
		for (const char Character : InUnit)
		{
			Format += Character;
			if (Character == '%')
			{
				Format += '%';
			}
		}
	}
	const std::array Colors{ImVec4(.95f, .3f, .3f, 1), ImVec4(.45f, .8f, .3f, 1), ImVec4(.3f, .6f, 1, 1)};
	float Values[]{InValue.X, InValue.Y, InValue.Z};
	bool bChanged{};
	for (unsigned Axis = 0; Axis < 3; ++Axis)
	{
		if (!bStacked || Axis == 0)
		{
			ImGui::SameLine(Start + (bStacked ? 0 : Axis * (Width + Spacing * 2)));
		}
		else
		{
			ImGui::SetCursorPosX(Start);
		}
		ImGui::PushID(int(Axis));
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(Colors[Axis], "%c", 'X' + Axis);
		const float AxisWidth = ImGui::GetItemRectSize().x;
		ImGui::SameLine(0, Spacing);
		ImGui::SetNextItemWidth(std::max(Scale(16), Width - AxisWidth - Spacing));
		const bool bEdited = Impl->DragNumber("##value", ImGuiDataType_Float, &Values[Axis], .01f, Format.c_str(),
		                                      nullptr, nullptr, InMixed[Axis]);
		bChanged |= bEdited;
		if (OutEdited)
		{
			(*OutEdited)[Axis] = bEdited;
		}
		const auto Minimum = ImGui::GetItemRectMin();
		const auto Maximum = ImGui::GetItemRectMax();
		OutBounds[Axis] = {Minimum.x, Minimum.y, Maximum.x, Maximum.y};
		if (!InTooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%.*s", int(InTooltip.size()), InTooltip.data());
		}
		ImGui::PopID();
	}
	EndPropertyRow();
	ImGui::EndGroup();
	if (bChanged)
	{
		InValue = {Values[0], Values[1], Values[2]};
	}
	return bChanged;
}
} // namespace Hyperion
