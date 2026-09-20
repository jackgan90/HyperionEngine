#include "GuiInternal.h"
#include <algorithm>
#include <imgui_internal.h>

namespace Hyperion
{
bool FGui::EditMixedScalar(FArchiveNode& InValue, const FRecordValueShape& InShape,
                           const FPropertyPresentation& InPresentation, bool bInMixed)
{
	Impl->Select();
	const char* Label = "##value";
	if (!InPresentation.Choices.empty())
	{
		auto Index = ReadInteger<std::size_t>(InValue);
		const auto Id = ImGui::GetID(Label);
		const char* Preview = bInMixed ? "Multiple Values" : InPresentation.Choices.at(Index).c_str();
		const bool bOpen = ImGui::BeginCombo(Label, Preview);
		const bool bActivated = ImGui::IsItemActivated();
		bool bChanged{};
		if (bOpen)
		{
			for (std::size_t Choice = 0; Choice < InPresentation.Choices.size(); ++Choice)
			{
				if (ImGui::Selectable(InPresentation.Choices[Choice].c_str(), !bInMixed && Choice == Index))
				{
					InValue = InShape.Kind == ERecordValueKind::Integer ? WriteValue(std::int64_t(Choice))
					                                                    : WriteValue(std::uint64_t(Choice));
					bChanged = true;
				}
			}
			ImGui::EndCombo();
		}
		return Impl->TrackEdit(Id, bOpen && !bChanged, bActivated, bChanged);
	}
	switch (InShape.Kind)
	{
		case ERecordValueKind::Boolean:
		{
			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, bInMixed);
			const bool bChanged =
			    Checkbox(bInMixed ? "Multiple Values###value" : "###value", std::get<bool>(InValue.Value));
			ImGui::PopItemFlag();
			return bChanged;
		}
		case ERecordValueKind::Number:
			return Impl->DragNumber(Label, ImGuiDataType_Double, &std::get<double>(InValue.Value), .01f, "%.9g",
			                        nullptr, nullptr, bInMixed);
		case ERecordValueKind::Integer:
			return Impl->DragNumber(Label, ImGuiDataType_S64, &std::get<std::int64_t>(InValue.Value), 1, nullptr,
			                        nullptr, nullptr, bInMixed);
		case ERecordValueKind::UnsignedInteger:
			return Impl->DragNumber(Label, ImGuiDataType_U64, &std::get<std::uint64_t>(InValue.Value), 1, nullptr,
			                        nullptr, nullptr, bInMixed);
		case ERecordValueKind::String:
		{
			auto& Value = std::get<std::string>(InValue.Value);
			std::vector<char> Buffer(Value.size() + 1024, 0);
			if (!bInMixed)
			{
				std::copy(Value.begin(), Value.end(), Buffer.begin());
			}
			const bool bSubmitted = bInMixed && ImGui::GetActiveID() == ImGui::GetID(Label) &&
			                        (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
			const bool bChanged = ImGui::InputTextWithHint(Label, bInMixed ? "Multiple Values" : "", Buffer.data(),
			                                               Buffer.size(), Impl->InputFlags());
			if (Impl->TrackEdit(bChanged || bSubmitted))
			{
				Value = Buffer.data();
				return true;
			}
			return false;
		}
		default:
			return false;
	}
}
} // namespace Hyperion
