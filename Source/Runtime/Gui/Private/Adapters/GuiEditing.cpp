#include "GuiInternal.h"
#include <imgui_internal.h>

namespace Hyperion
{
bool DragNumericValue(const char* InLabel, ImGuiDataType InType, void* InValue, float InSpeed, const char* InFormat,
                      const void* InMinimum, const void* InMaximum)
{
	const bool bChanged = ImGui::DragScalar(InLabel, InType, InValue, InSpeed, InMinimum, InMaximum, InFormat,
	                                        ImGuiSliderFlags_NoRoundToFormat | ImGuiSliderFlags_AlwaysClamp);
	const bool bTextInput = ImGui::TempInputIsActive(ImGui::GetItemID());
	if (!bTextInput && ImGui::IsItemActive() && ImGui::IsMouseDown(0))
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
	}
	else if (ImGui::IsItemHovered() && !bTextInput)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	return bChanged;
}

bool FGui::FImpl::DragNumber(const char* InLabel, ImGuiDataType InType, void* InValue, float InSpeed,
                             const char* InFormat, const void* InMinimum, const void* InMaximum)
{
	return TrackEdit(DragNumericValue(InLabel, InType, InValue, InSpeed, InFormat, InMinimum, InMaximum));
}

bool FGui::FImpl::DragComponents(const char* InLabel, float* InValues, int InCount)
{
	ImGui::BeginGroup();
	ImGui::PushID(InLabel);
	ImGui::PushMultiItemsWidths(InCount, ImGui::CalcItemWidth());
	bool bChanged{};
	for (int Index = 0; Index < InCount; ++Index)
	{
		ImGui::PushID(Index);
		if (Index != 0)
		{
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		}
		bChanged |= DragNumber("##value", ImGuiDataType_Float, &InValues[Index], .01f, "%.6g");
		ImGui::PopID();
		ImGui::PopItemWidth();
	}
	ImGui::PopID();
	const char* End = ImGui::FindRenderedTextEnd(InLabel);
	if (End != InLabel)
	{
		ImGui::SameLine();
		ImGui::TextUnformatted(InLabel, End);
	}
	ImGui::EndGroup();
	return bChanged;
}

bool FGui::FImpl::TrackEdit(bool bInChanged)
{
	return TrackEdit(ImGui::GetItemID(), ImGui::IsItemActive(), ImGui::IsItemActivated(), bInChanged);
}

bool FGui::FImpl::TrackEdit(ImGuiID InId, bool bInActive, bool bInActivated, bool bInChanged)
{
	if (bLiveEdit && (bInActive || bInChanged))
	{
		const int Frame = ImGui::GetFrameCount();
		if (EditItem != InId || bInActivated || EditFrame < Frame - 1)
		{
			++EditSerial;
		}
		EditItem = InId;
		EditFrame = Frame;
		if (bInActive)
		{
			EditState.ActiveInteraction = EditSerial;
		}
		if (bInChanged)
		{
			EditState.ChangedInteraction = EditSerial;
		}
	}
	return bInChanged;
}

EMouseCursor FGui::MouseCursor() const
{
	Impl->Select();
	switch (ImGui::GetMouseCursor())
	{
		case ImGuiMouseCursor_None:
			return EMouseCursor::Hidden;
		case ImGuiMouseCursor_TextInput:
			return EMouseCursor::TextInput;
		case ImGuiMouseCursor_ResizeAll:
			return EMouseCursor::ResizeAll;
		case ImGuiMouseCursor_ResizeNS:
			return EMouseCursor::ResizeVertical;
		case ImGuiMouseCursor_ResizeEW:
			return EMouseCursor::ResizeHorizontal;
		case ImGuiMouseCursor_ResizeNESW:
			return EMouseCursor::ResizeDiagonalNE;
		case ImGuiMouseCursor_ResizeNWSE:
			return EMouseCursor::ResizeDiagonalNW;
		case ImGuiMouseCursor_Hand:
			return EMouseCursor::Hand;
		case ImGuiMouseCursor_NotAllowed:
			return EMouseCursor::NotAllowed;
		default:
			return EMouseCursor::Arrow;
	}
}

void FGui::BeginLiveEdit()
{
	Impl->Select();
	ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInput, true);
	Impl->bLiveEdit = true;
	Impl->EditState = {};
}

FGuiEditState FGui::EndLiveEdit()
{
	Impl->Select();
	ImGui::PopItemFlag();
	Impl->bLiveEdit = false;
	return Impl->EditState;
}

void FGui::FinishEditing()
{
	Impl->Select();
	ImGui::ClearActiveID();
	auto& Context = *ImGui::GetCurrentContext();
	// Deactivation normally reapplies the last text buffer. History has replaced that value.
	Context.InputTextDeactivatedState.ID = 0;
	Context.InputTextState.ID = 0;
	if (!Context.OpenPopupStack.empty())
	{
		ImGui::ClosePopupToLevel(0, true);
	}
	Impl->EditItem = 0;
	Impl->EditState = {};
}
} // namespace Hyperion
