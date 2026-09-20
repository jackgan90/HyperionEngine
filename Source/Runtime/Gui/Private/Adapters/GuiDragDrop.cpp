#include "GuiInternal.h"
#include <cstring>
#include <imgui_internal.h>

namespace Hyperion
{
bool FGui::DragSource(const char* InType, std::string_view InValue, const char* InLabel)
{
	Impl->Select();
	if (!InType || !*InType || std::strlen(InType) > 31 || InValue.empty() || InValue.size() > 4096)
	{
		throw std::invalid_argument("Invalid GUI drag payload");
	}
	if (Impl->bDragCancelled ||
	    !ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_SourceAllowNullID))
	{
		return false;
	}
	ImGui::SetDragDropPayload(InType, InValue.data(), InValue.size(), ImGuiCond_Once);
	ImGui::TextUnformatted(InLabel);
	ImGui::EndDragDropSource();
	return true;
}

std::optional<FGuiDragPayload> FGui::DragPayload() const
{
	Impl->Select();
	const auto* Payload = ImGui::GetDragDropPayload();
	if (!Payload || Impl->bDragCancelled)
	{
		return {};
	}
	return FGuiDragPayload{Payload->DataType,
	                       {static_cast<const char*>(Payload->Data), static_cast<std::size_t>(Payload->DataSize)},
	                       Payload->IsDelivery()};
}

std::optional<FGuiDragPayload> FGui::DropTarget(const char* InType)
{
	Impl->Select();
	std::optional<FGuiDragPayload> Result;
	if (!Impl->bDragCancelled && ImGui::BeginDragDropTarget())
	{
		if (const auto* Payload = ImGui::AcceptDragDropPayload(InType, ImGuiDragDropFlags_AcceptBeforeDelivery |
		                                                                   ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
		{
			Result =
			    FGuiDragPayload{InType,
			                    {static_cast<const char*>(Payload->Data), static_cast<std::size_t>(Payload->DataSize)},
			                    Payload->IsDelivery()};
		}
		ImGui::EndDragDropTarget();
	}
	return Result;
}

void FGui::CancelDragDrop()
{
	Impl->Select();
	if (ImGui::GetDragDropPayload())
	{
		ImGui::ClearDragDrop();
		ImGui::ClearActiveID();
		Impl->bDragCancelled = true;
	}
}

void FGui::Image(std::uint64_t InTextureId, FVec2 InSize)
{
	Impl->Select();
	ImGui::Image(ImTextureID(InTextureId), {Scale(InSize.X), Scale(InSize.Y)});
}

void FGui::FocusWindow(const char* InTitle)
{
	Impl->Select();
	ImGui::SetWindowFocus(InTitle);
}

void FGui::DrawImageOverlay(std::uint64_t InTextureId, FVec4 InClip, FVec4 InBounds, FVec4 InTint)
{
	Impl->Select();
	auto* Draw = ImGui::GetWindowDrawList();
	Draw->PushClipRect({InClip.X, InClip.Y}, {InClip.Z, InClip.W}, true);
	Draw->AddImage(ImTextureID(InTextureId), {InBounds.X, InBounds.Y}, {InBounds.Z, InBounds.W}, {0, 0}, {1, 1},
	               ImGui::ColorConvertFloat4ToU32({InTint.X, InTint.Y, InTint.Z, InTint.W}));
	Draw->PopClipRect();
}
} // namespace Hyperion
