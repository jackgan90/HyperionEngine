#include "GuiInternal.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
float ToSrgb(float InLinear)
{
	const float Value = std::clamp(InLinear, 0.f, 1.f);
	return Value <= .0031308f ? Value * 12.92f : 1.055f * std::pow(Value, 1.f / 2.4f) - .055f;
}

float ToLinear(float InSrgb)
{
	return InSrgb <= .04045f ? InSrgb / 12.92f : std::pow((InSrgb + .055f) / 1.055f, 2.4f);
}

void Observe(const std::function<void(std::string_view, FVec4)>& InObserve, std::string_view InId)
{
	if (InObserve)
	{
		const auto Minimum = ImGui::GetItemRectMin();
		const auto Maximum = ImGui::GetItemRectMax();
		InObserve(InId, {Minimum.x, Minimum.y, Maximum.x, Maximum.y});
	}
}

bool ColorPickerValues(float* InColor, const std::function<void(std::string_view, FVec4)>& InObserve, float InScale)
{
	ImGui::SetNextItemWidth(300 * InScale);
	bool bChanged = ImGui::ColorPicker3("##picker", InColor,
	                                    ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueWheel |
	                                        ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoAlpha |
	                                        ImGuiColorEditFlags_NoInputs);
	Observe(InObserve, "picker");
	const std::array Labels{"R", "G", "B"};
	const int Minimum = 0;
	const int Maximum = 255;
	for (unsigned Channel = 0; Channel < 3; ++Channel)
	{
		int Value = int(std::lround(InColor[Channel] * 255));
		ImGui::PushID(Labels[Channel]);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Labels[Channel]);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(280 * InScale);
		if (DragNumericValue("##value", ImGuiDataType_S32, &Value, 1, "%d", &Minimum, &Maximum))
		{
			InColor[Channel] = float(Value) / 255;
			bChanged = true;
		}
		ImGui::PopID();
	}
	return bChanged;
}

bool ColorPopup(FVec3& InValue, ImGuiStorage& InStorage, const std::array<ImGuiID, 4>& InKeys,
                const std::function<void(std::string_view, FVec4)>& InObserve, bool bInLive, float InScale)
{
	if (!ImGui::BeginPopup("Color picker"))
	{
		return false;
	}
	float Color[]{InStorage.GetFloat(InKeys[0]), InStorage.GetFloat(InKeys[1]), InStorage.GetFloat(InKeys[2])};
	bool bChanged{};
	ImGui::TextUnformatted("Color picker");
	if (ColorPickerValues(Color, InObserve, InScale))
	{
		for (unsigned Channel = 0; Channel < 3; ++Channel)
		{
			InStorage.SetFloat(InKeys[Channel], Color[Channel]);
		}
		InStorage.SetBool(InKeys[3], true);
		if (bInLive)
		{
			InValue = {ToLinear(Color[0]), ToLinear(Color[1]), ToLinear(Color[2])};
			bChanged = true;
		}
	}
	if (ImGui::Button(bInLive ? "Close" : "OK"))
	{
		// Opening and accepting must not quantize or clamp an untouched source color.
		bChanged |= !bInLive && InStorage.GetBool(InKeys[3]);
		if (bChanged)
		{
			InValue = {ToLinear(Color[0]), ToLinear(Color[1]), ToLinear(Color[2])};
		}
		ImGui::CloseCurrentPopup();
	}
	Observe(InObserve, "accept");
	ImGui::SameLine();
	if (!bInLive && ImGui::Button("Cancel"))
	{
		ImGui::CloseCurrentPopup();
	}
	Observe(InObserve, "cancel");
	ImGui::EndPopup();
	return bChanged;
}
} // namespace

bool FGui::InputColor(const char* InLabel, FVec3& InValue,
                      const std::function<void(std::string_view, FVec4)>& InObserve)
{
	Impl->Select();
	ImGui::BeginGroup();
	bool bExpanded{};
	BeginPropertyRow(InLabel, &bExpanded);
	Observe(InObserve, "expand");
	const ImGuiID Id = ImGui::GetID("##swatch");
	bool bOpened{};
	auto& Storage = *ImGui::GetStateStorage();
	const std::array Keys{ImGui::GetID("picker-r"), ImGui::GetID("picker-g"), ImGui::GetID("picker-b"),
	                      ImGui::GetID("picker-modified")};
	const float Color[]{ToSrgb(InValue.X), ToSrgb(InValue.Y), ToSrgb(InValue.Z)};
	if (ImGui::ColorButton("##swatch", {Color[0], Color[1], Color[2], 1},
	                       ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoTooltip,
	                       {std::max(1.f, ImGui::GetContentRegionAvail().x), ImGui::GetFrameHeight()}))
	{
		for (unsigned Channel = 0; Channel < 3; ++Channel)
		{
			Storage.SetFloat(Keys[Channel], Color[Channel]);
		}
		Storage.SetBool(Keys[3], false);
		ImGui::OpenPopup("Color picker");
		bOpened = true;
	}
	Observe(InObserve, "swatch");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("sRGB: %d, %d, %d", int(std::lround(Color[0] * 255)), int(std::lround(Color[1] * 255)),
		                  int(std::lround(Color[2] * 255)));
	}
	bool bChanged = ColorPopup(InValue, Storage, Keys, InObserve, Impl->bLiveEdit, Impl->AppliedScale);
	Impl->TrackEdit(Id, ImGui::IsPopupOpen("Color picker"), bOpened, bChanged);
	if (bExpanded)
	{
		const std::array Labels{"R", "G", "B"};
		const std::array Channels{&InValue.X, &InValue.Y, &InValue.Z};
		for (unsigned Channel = 0; Channel < 3; ++Channel)
		{
			BeginPropertyRow(Labels[Channel]);
			int Value = int(std::lround(ToSrgb(*Channels[Channel]) * 255));
			const int Minimum = 0;
			const int Maximum = 255;
			if (Impl->DragNumber("##value", ImGuiDataType_S32, &Value, 1, "%d", &Minimum, &Maximum))
			{
				*Channels[Channel] = ToLinear(float(std::clamp(Value, 0, 255)) / 255);
				bChanged = true;
			}
			Observe(InObserve, Labels[Channel]);
			EndPropertyRow();
		}
	}
	EndPropertyRow();
	ImGui::EndGroup();
	return bChanged;
}
} // namespace Hyperion
