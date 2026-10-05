#include "GuiInternal.h"

namespace Hyperion
{
namespace
{
const char* ChoiceLabel(const std::string& InChoice)
{
	return InChoice.c_str();
}

const char* ChoiceLabel(const FPropertyChoice& InChoice)
{
	return InChoice.Label.c_str();
}
} // namespace

template<class T>
bool FGui::FImpl::Combo(FGui& InGui, const char* InLabel, std::span<const T> InChoices, std::size_t& InIndex,
                        const std::function<void(std::size_t, FVec4)>& InObserve, const char* InPreview)
{
	Select();
	bool bChanged{};
	const char* Preview = InPreview ? InPreview : InIndex < InChoices.size() ? ChoiceLabel(InChoices[InIndex]) : "None";
	const ImGuiID Id = ImGui::GetID(InLabel);
	const bool bOpen = ImGui::BeginCombo(InLabel, Preview);
	const bool bActivated = ImGui::IsItemActivated();
	if (bOpen)
	{
		for (std::size_t Index = 0; Index < InChoices.size(); ++Index)
		{
			ImGui::PushID(static_cast<int>(Index));
			if (ImGui::Selectable(ChoiceLabel(InChoices[Index]), Index == InIndex))
			{
				InIndex = Index;
				bChanged = true;
			}
			ImGui::PopID();
			if (InObserve)
			{
				InObserve(Index, InGui.LastItemBounds());
			}
		}
		ImGui::EndCombo();
	}
	return TrackEdit(Id, bOpen && !bChanged, bActivated, bChanged);
}

bool FGui::Combo(const char* InLabel, std::span<const std::string> InChoices, std::size_t& InIndex,
                 const std::function<void(std::size_t, FVec4)>& InObserve, const char* InPreview)
{
	return Impl->Combo(*this, InLabel, InChoices, InIndex, InObserve, InPreview);
}

bool FGui::Combo(const char* InLabel, std::span<const FPropertyChoice> InChoices, std::size_t& InIndex,
                 const std::function<void(std::size_t, FVec4)>& InObserve, const char* InPreview)
{
	return Impl->Combo(*this, InLabel, InChoices, InIndex, InObserve, InPreview);
}
} // namespace Hyperion
