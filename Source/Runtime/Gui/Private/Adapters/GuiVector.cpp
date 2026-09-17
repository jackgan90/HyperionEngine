#include "GuiInternal.h"
#include <algorithm>

namespace Hyperion
{
bool FGui::InputVectorRow(const char* InLabel, FVec3& InValue, std::string_view InUnit, std::string_view InTooltip,
                          std::array<FVec4, 3>& OutBounds)
{
	Impl->Select();
	ImGui::PushID(InLabel);
	ImGui::BeginGroup();
	const float Start = ImGui::GetCursorPosX();
	const float Spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const float LabelWidth = 76;
	const float Width = std::max(24.f, (ImGui::GetContentRegionAvail().x - LabelWidth - Spacing * 5) / 3);
	const std::string_view Label(InLabel);
	const auto Marker = Label.find("##");
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(InLabel, InLabel + (Marker == std::string_view::npos ? Label.size() : Marker));
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
		ImGui::SameLine(Start + LabelWidth + Axis * (Width + Spacing * 2));
		ImGui::PushID(int(Axis));
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(Colors[Axis], "%c", 'X' + Axis);
		const float AxisWidth = ImGui::GetItemRectSize().x;
		ImGui::SameLine(0, Spacing);
		ImGui::SetNextItemWidth(std::max(16.f, Width - AxisWidth - Spacing));
		bChanged |= ImGui::InputScalar("##value", ImGuiDataType_Float, &Values[Axis], nullptr, nullptr, Format.c_str(),
		                               ImGuiInputTextFlags_EnterReturnsTrue);
		const auto Minimum = ImGui::GetItemRectMin();
		const auto Maximum = ImGui::GetItemRectMax();
		OutBounds[Axis] = {Minimum.x, Minimum.y, Maximum.x, Maximum.y};
		if (!InTooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%.*s", int(InTooltip.size()), InTooltip.data());
		}
		ImGui::PopID();
	}
	ImGui::EndGroup();
	ImGui::PopID();
	if (bChanged)
	{
		InValue = {Values[0], Values[1], Values[2]};
	}
	return bChanged;
}
} // namespace Hyperion
