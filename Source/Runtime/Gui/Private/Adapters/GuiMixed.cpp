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
		auto Index = bInMixed
		                 ? InPresentation.Choices.size()
		                 : PropertyChoiceIndex(InPresentation.Choices, InValue).value_or(InPresentation.Choices.size());
		if (Combo(Label, InPresentation.Choices, Index, {}, bInMixed ? "Multiple Values" : nullptr))
		{
			InValue = InPresentation.Choices.at(Index).Value;
			return true;
		}
		return false;
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
			const bool bPath = InPresentation.Widget == EPropertyWidget::Path;
			const auto Display = bPath ? Impl->PathDisplay.Value(Value)
			                     : (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled)
			                         ? Impl->PathDisplay.Text(Value)
			                         : Value;
			std::vector<char> Buffer(Display.size() + 1024, 0);
			if (!bInMixed)
			{
				std::copy(Display.begin(), Display.end(), Buffer.begin());
			}
			const bool bSubmitted = bInMixed && ImGui::GetActiveID() == ImGui::GetID(Label) &&
			                        (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
			const bool bChanged = ImGui::InputTextWithHint(Label, bInMixed ? "Multiple Values" : "", Buffer.data(),
			                                               Buffer.size(), Impl->InputFlags());
			if (Impl->TrackEdit(bChanged || bSubmitted))
			{
				Value = bPath ? Impl->PathDisplay.Resolve(Buffer.data()) : Buffer.data();
				return true;
			}
			return false;
		}
		default:
			return false;
	}
}
} // namespace Hyperion
