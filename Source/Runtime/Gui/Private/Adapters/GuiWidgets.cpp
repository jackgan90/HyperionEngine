#include "GuiInternal.h"
#include <algorithm>
#include <imgui_internal.h>

namespace Hyperion
{
bool FGui::InputNumber(const char* InLabel, double& InValue)
{
	Impl->Select();
	return Impl->DragNumber(InLabel, ImGuiDataType_Double, &InValue, .01f, "%.9g");
}

bool FGui::InputInteger(const char* InLabel, std::int64_t& InValue)
{
	Impl->Select();
	return Impl->DragNumber(InLabel, ImGuiDataType_S64, &InValue, 1);
}

bool FGui::InputInteger(const char* InLabel, std::uint64_t& InValue)
{
	Impl->Select();
	return Impl->DragNumber(InLabel, ImGuiDataType_U64, &InValue, 1);
}

void FGui::BeginPropertyRow(const char* InLabel, bool* bOutExpanded, const char* InTooltip,
                            std::span<const FPropertyTooltipLine> InTooltipLines, const char* InWarningTooltip)
{
	Impl->Select();
	ImGui::PushID(InLabel);
	const float Start = ImGui::GetCursorPosX();
	const float Available = ImGui::GetContentRegionAvail().x;
	const std::string_view Label(InLabel);
	const auto Marker = Label.find("##");
	const char* End = InLabel + (Marker == std::string_view::npos ? Label.size() : Marker);
	const bool bHasTooltip = InTooltip || !InTooltipLines.empty();
	const float HelpWidth = bHasTooltip ? ImGui::CalcTextSize("(?)").x + ImGui::GetStyle().ItemInnerSpacing.x : 0;
	const bool bHasWarning = InWarningTooltip && *InWarningTooltip;
	const float WarningWidth = bHasWarning ? ImGui::CalcTextSize("[!]").x + ImGui::GetStyle().ItemInnerSpacing.x : 0;
	const float LabelWidth =
	    std::max(ImGui::CalcTextSize(InLabel, End).x + WarningWidth + HelpWidth + ImGui::GetStyle().ItemInnerSpacing.x,
	             std::clamp(Available * .38f, Scale(76), Scale(160)));
	ImGui::AlignTextToFramePadding();
	if (bOutExpanded)
	{
		*bOutExpanded =
		    ImGui::TreeNodeEx("##expand", ImGuiTreeNodeFlags_NoTreePushOnOpen, "%.*s", int(End - InLabel), InLabel);
	}
	else
	{
		ImGui::TextUnformatted(InLabel, End);
	}
	if (bHasWarning)
	{
		ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextUnformatted("[!]");
		Tooltip(InWarningTooltip);
	}
	if (bHasTooltip)
	{
		ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextDisabled("(?)");
		Tooltip(InTooltip, InTooltipLines);
	}
	// SameLine offsets are group-relative; Start is already window-relative, including indentation.
	ImGui::SameLine();
	ImGui::SetCursorPosX(Start + LabelWidth);
	ImGui::SetNextItemWidth(std::max(1.f, ImGui::GetContentRegionAvail().x));
}

void FGui::EndPropertyRow()
{
	Impl->Select();
	ImGui::PopID();
}

bool FGui::InputPathWithBrowse(const char* InLabel, std::string& InValue)
{
	BeginPropertyRow(InLabel);
	const auto& Style = ImGui::GetStyle();
	const float ButtonWidth = ImGui::CalcTextSize("Browse").x + Style.FramePadding.x * 2;
	ImGui::SetNextItemWidth(std::max(1.f, ImGui::GetContentRegionAvail().x - ButtonWidth - Style.ItemSpacing.x));
	InputText("##path", InValue, false, true);
	ImGui::SameLine();
	const bool bBrowse = ImGui::Button("Browse");
	EndPropertyRow();
	return bBrowse;
}

void FGui::BeginDisabled(bool bInDisabled)
{
	Impl->Select();
	ImGui::BeginDisabled(bInDisabled);
}

void FGui::EndDisabled()
{
	Impl->Select();
	ImGui::EndDisabled();
}

bool FGui::BeginMenuBar()
{
	Impl->Select();
	return ImGui::BeginMainMenuBar();
}

void FGui::EndMenuBar()
{
	Impl->Select();
	ImGui::EndMainMenuBar();
}

bool FGui::BeginMenu(const char* InLabel)
{
	Impl->Select();
	return ImGui::BeginMenu(InLabel);
}

void FGui::EndMenu()
{
	Impl->Select();
	ImGui::EndMenu();
}

bool FGui::MenuItem(const char* InLabel, const char* InShortcut, bool bInSelected)
{
	Impl->Select();
	return ImGui::MenuItem(InLabel, InShortcut, bInSelected);
}

void FGui::SameLine()
{
	Impl->Select();
	ImGui::SameLine();
}

void FGui::SetNextItemWidth(float InWidth)
{
	Impl->Select();
	ImGui::SetNextItemWidth(InWidth > 0 ? Scale(InWidth) : InWidth);
}

void FGui::SameLineIfFits(const char* InButtonLabel)
{
	Impl->Select();
	const auto& Style = ImGui::GetStyle();
	const float Right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
	const float Width = ImGui::CalcTextSize(InButtonLabel, nullptr, true).x + Style.FramePadding.x * 2;
	if (ImGui::GetItemRectMax().x + Style.ItemSpacing.x + Width <= Right)
	{
		ImGui::SameLine();
	}
}

bool FGui::Section(const char* InLabel, bool bInDefaultOpen, bool* bOutClose, const char* InCloseTooltip)
{
	Impl->Select();
	bool bVisible = true;
	ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_TableHeaderBg));
	const bool bOpen = ImGui::CollapsingHeader(InLabel, bOutClose ? &bVisible : nullptr,
	                                           bInDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
	ImGui::PopStyleColor();
	if (bOutClose)
	{
		*bOutClose = !bVisible;
		// CollapsingHeader restores the header as LastItem; target its nested close button for help.
		const auto CloseId = ImGui::GetIDWithSeed("#CLOSE", nullptr, ImGui::GetID(InLabel));
		if (InCloseTooltip && ImGui::GetHoveredID() == CloseId)
		{
			const auto HeaderItem = GImGui->LastItemData;
			ImGui::SetTooltip("%s", InCloseTooltip);
			GImGui->LastItemData = HeaderItem;
		}
	}
	return bOpen;
}

void FGui::Indent()
{
	Impl->Select();
	ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
}

void FGui::Unindent()
{
	Impl->Select();
	ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
}

bool FGui::BeginTable(const char* InId, const char* InFirst, const char* InSecond)
{
	Impl->Select();
	if (!ImGui::BeginTable(InId, 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
	{
		return false;
	}
	ImGui::TableSetupColumn(InFirst, ImGuiTableColumnFlags_WidthStretch, 2);
	ImGui::TableSetupColumn(InSecond, ImGuiTableColumnFlags_WidthStretch, 1);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();
	return true;
}

void FGui::NextRow()
{
	Impl->Select();
	ImGui::TableNextRow();
}

void FGui::NextColumn()
{
	Impl->Select();
	ImGui::TableNextColumn();
}

void FGui::EndTable()
{
	Impl->Select();
	ImGui::EndTable();
}

bool FGui::TreeItem(const char* InId, const char* InLabel, bool bInLeaf, bool bInSelected, bool& bOutClicked,
                    bool bInDefaultOpen, FVec4* OutToggleBounds)
{
	Impl->Select();
	const auto Origin = ImGui::GetCursorScreenPos();
	const auto Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAllColumns |
	                   (bInDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0) | (bInLeaf ? ImGuiTreeNodeFlags_Leaf : 0) |
	                   (bInSelected ? ImGuiTreeNodeFlags_Selected : 0);
	const bool bOpen = ImGui::TreeNodeEx(InId, Flags, "%s", InLabel);
	bOutClicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
	if (OutToggleBounds)
	{
		const auto& Style = ImGui::GetStyle();
		*OutToggleBounds = bInLeaf ? FVec4{}
		                           : FVec4{Origin.x - Style.TouchExtraPadding.x, ImGui::GetItemRectMin().y,
		                                   Origin.x + ImGui::GetTreeNodeToLabelSpacing() + Style.TouchExtraPadding.x,
		                                   ImGui::GetItemRectMax().y};
	}
	return bOpen;
}

void FGui::EndTree()
{
	Impl->Select();
	ImGui::TreePop();
}

void FGui::OpenNextTreeItem()
{
	Impl->Select();
	ImGui::SetNextItemOpen(true);
}

bool FGui::IsItemPressed() const
{
	Impl->Select();
	return ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
}

void FGui::Property(const char* InLabel, const std::string& InValue)
{
	Impl->Select();
	ImGui::TextDisabled("%s", InLabel);
	ImGui::SameLine(std::max(Scale(90), ImGui::GetContentRegionAvail().x * .38f));
	ImGui::TextUnformatted(InValue.c_str());
}

FGuiImageRegion FGui::Image(std::uint64_t InTextureId)
{
	Impl->Select();
	const auto CaptureId = ImGui::GetID("##ImageOverlayPointer");
	if (Impl->ImagePointerCapture == CaptureId)
	{
		ImGui::KeepAliveID(CaptureId);
	}
	const auto Available = ImGui::GetContentRegionAvail();
	if (InTextureId)
	{
		ImGui::Image(ImTextureID(InTextureId), {std::max(1.f, Available.x), std::max(1.f, Available.y)});
	}
	else
	{
		ImGui::Dummy({std::max(1.f, Available.x), std::max(1.f, Available.y)});
		ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
		                                          IM_COL32(22, 22, 22, 255));
	}
	const auto Minimum = ImGui::GetItemRectMin();
	const auto Maximum = ImGui::GetItemRectMax();
	// Images have no input ID by default: a held click becomes a window-background drag.
	// Register the image as an interactive item so ownership survives until release.
	const ImRect Bounds(Minimum, Maximum);
	if (ImGui::ItemAdd(Bounds, CaptureId, nullptr, ImGuiItemFlags_NoNav))
	{
		bool bButtonHovered{};
		bool bButtonHeld{};
		ImGui::ButtonBehavior(Bounds, CaptureId, &bButtonHovered, &bButtonHeld, ImGuiButtonFlags_NoNavFocus);
	}
	const bool bHovered = ImGui::IsItemHovered();
	if (bHovered && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
	{
		ImGui::SetWindowFocus();
	}
	const bool bFocused = ImGui::IsWindowFocused() &&
	                      (!ImGui::IsAnyItemActive() || ImGui::GetActiveID() == CaptureId) &&
	                      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	ImGui::GetWindowDrawList()->AddRect(Minimum, Maximum,
	                                    bFocused ? IM_COL32(206, 153, 50, 255) : IM_COL32(45, 45, 45, 255));
	return {{Minimum.x, Minimum.y, Maximum.x, Maximum.y}, bHovered, bFocused};
}

FGuiPointerState FGui::PointerState() const
{
	Impl->Select();
	const auto Position = ImGui::GetMousePos();
	return {{Position.x, Position.y},
	        ImGui::IsMouseClicked(0),
	        ImGui::IsMouseDown(0),
	        ImGui::IsMouseReleased(0),
	        ImGui::IsKeyPressed(ImGuiKey_Escape),
	        ImGui::IsMouseDown(1),
	        ImGui::IsMousePosValid(&Position),
	        ImGui::GetIO().KeyCtrl,
	        ImGui::GetIO().KeyShift};
}

void FGui::CaptureImagePointer(bool bInCapture)
{
	Impl->Select();
	if (bInCapture)
	{
		Impl->ImagePointerCapture = ImGui::GetID("##ImageOverlayPointer");
		ImGui::SetActiveID(Impl->ImagePointerCapture, ImGui::GetCurrentWindow());
		ImGui::KeepAliveID(Impl->ImagePointerCapture);
	}
	else
	{
		if (Impl->ImagePointerCapture && ImGui::GetActiveID() == Impl->ImagePointerCapture)
		{
			ImGui::ClearActiveID();
		}
		Impl->ImagePointerCapture = 0;
	}
}

void FGui::DrawImageOverlay(FVec4 InClip, std::span<const FVec2> InPoints, FVec4 InColor, float InThickness,
                            bool bInFilled)
{
	Impl->Select();
	if (InPoints.size() < 2)
	{
		return;
	}
	std::vector<ImVec2> Points;
	Points.reserve(InPoints.size());
	for (const auto Point : InPoints)
	{
		Points.emplace_back(Point.X, Point.Y);
	}
	auto* Draw = ImGui::GetWindowDrawList();
	Draw->PushClipRect({InClip.X, InClip.Y}, {InClip.Z, InClip.W}, true);
	const auto Color = ImGui::ColorConvertFloat4ToU32({InColor.X, InColor.Y, InColor.Z, InColor.W});
	if (bInFilled && Points.size() >= 3)
	{
		Draw->AddConvexPolyFilled(Points.data(), static_cast<int>(Points.size()), Color);
	}
	else
	{
		Draw->AddPolyline(Points.data(), static_cast<int>(Points.size()), Color, ImDrawFlags_None, InThickness);
	}
	Draw->PopClipRect();
}

void FGui::OpenPopup(const char* InTitle)
{
	Impl->Select();
	ImGui::OpenPopup(InTitle);
}

bool FGui::CenteredButton(const char* InLabel, bool bInEnabled)
{
	Impl->Select();
	const float Width = ImGui::CalcTextSize(InLabel, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, (ImGui::GetContentRegionAvail().x - Width) * .5f));
	return Button(InLabel, bInEnabled);
}

bool FGui::ButtonInCenteredRow(std::span<const char* const> InLabels, std::size_t InIndex, bool bInEnabled)
{
	Impl->Select();
	if (InIndex >= InLabels.size())
	{
		throw std::out_of_range("Centered button index is outside the label list");
	}
	auto* Window = ImGui::GetCurrentWindow();
	const auto& Style = ImGui::GetStyle();
	const float Available = Window->WorkRect.GetWidth();
	float ContentRight = Window->DC.CursorMaxPos.x;
	std::size_t RowStart = 0;
	while (RowStart < InLabels.size())
	{
		std::size_t RowEnd = RowStart;
		float RowWidth = 0;
		while (RowEnd < InLabels.size())
		{
			const float Width = ImGui::CalcTextSize(InLabels[RowEnd], nullptr, true).x + Style.FramePadding.x * 2;
			const float NextWidth = RowWidth + (RowEnd > RowStart ? Style.ItemSpacing.x : 0) + Width;
			if (RowEnd > RowStart && NextWidth > Available)
			{
				break;
			}
			RowWidth = NextWidth;
			++RowEnd;
		}
		if (InIndex < RowEnd)
		{
			ContentRight = std::max(ContentRight, Window->WorkRect.Min.x + RowWidth);
			if (InIndex == RowStart)
			{
				ImGui::SetCursorPosX(Window->WorkRect.Min.x - Window->Pos.x +
				                     std::max(0.f, (Available - RowWidth) * .5f));
			}
			else
			{
				ImGui::SameLine();
			}
			break;
		}
		RowStart = RowEnd;
	}
	const bool bPressed = Button(InLabels[InIndex], bInEnabled);
	// Centering is presentation space, not extra content width for an auto-sized modal.
	Window->DC.CursorMaxPos.x = ContentRight;
	return bPressed;
}

void FGui::BeginActionLayout(const char* InId, const std::string& InMessage)
{
	Impl->Select();
	ImGui::PushID(InId);
	const auto Available = ImGui::GetContentRegionAvail();
	const float Spacing = ImGui::GetStyle().ItemSpacing.y;
	float FooterHeight = ImGui::GetFrameHeight() + Spacing;
	if (!InMessage.empty())
	{
		const auto Text = Impl->PathDisplay.Text(InMessage);
		const float TextHeight = ImGui::CalcTextSize(Text.c_str(), nullptr, false, std::max(1.f, Available.x)).y;
		FooterHeight += std::min(TextHeight, Available.y * .3f) + Spacing;
	}
	ImGui::BeginChild("Body", {0, std::max(1.f, Available.y - FooterHeight)});
}

bool FGui::EndActionLayout(const char* InLabel, const std::string& InMessage, bool bInEnabled)
{
	Impl->Select();
	ImGui::EndChild();
	const float FrameHeight = ImGui::GetFrameHeight();
	if (!InMessage.empty())
	{
		const float Height = ImGui::GetContentRegionAvail().y - FrameHeight - ImGui::GetStyle().ItemSpacing.y;
		ImGui::BeginChild("Message", {0, std::max(1.f, Height)});
		TextWrapped(InMessage);
		ImGui::EndChild();
	}
	const float Width = ImGui::CalcTextSize(InLabel, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2;
	const auto Available = ImGui::GetContentRegionAvail();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.f, Available.y - FrameHeight));
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, Available.x - Width));
	const bool bPressed = Button(InLabel, bInEnabled);
	ImGui::PopID();
	return bPressed;
}

bool FGui::BeginModal(const char* InTitle, bool& bInOpen, bool bInAutoResize)
{
	Impl->Select();
	const auto* Viewport = ImGui::GetMainViewport();
	ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse;
	if (bInAutoResize)
	{
		const float MaxWidth = std::min(Scale(660), Viewport->Size.x * .8f);
		// Seed wrapping with a readable width; subsequent frames fit the text and action rows.
		ImGui::SetNextWindowSize({MaxWidth, 0}, ImGuiCond_Appearing);
		ImGui::SetNextWindowSizeConstraints({0, 0}, {MaxWidth, Viewport->Size.y * .8f});
		Flags |= ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	}
	else
	{
		ImGui::SetNextWindowSize({Scale(660), Scale(460)}, ImGuiCond_FirstUseEver);
	}
	ImGui::SetNextWindowPos(Viewport->GetCenter(), bInAutoResize ? ImGuiCond_Always : ImGuiCond_Appearing, {.5f, .5f});
	return ImGui::BeginPopupModal(InTitle, &bInOpen, Flags);
}

bool FGui::BeginMessageModal(const char* InTitle, bool& bInOpen, const std::string& InMessage)
{
	Impl->Select();
	const auto* Viewport = ImGui::GetMainViewport();
	const auto& Style = ImGui::GetStyle();
	const float MaxWidth = std::min(Scale(560), Viewport->Size.x * .8f);
	const float TextWidth = ImGui::CalcTextSize(InMessage.c_str()).x + Style.WindowPadding.x * 2;
	const float TitleWidth = ImGui::CalcTextSize(InTitle).x + ImGui::GetFrameHeight() * 2;
	const float Width = std::clamp(std::max(TextWidth, TitleWidth), std::min(Scale(240), MaxWidth), MaxWidth);
	ImGui::SetNextWindowSize({Width, 0}, ImGuiCond_Always);
	ImGui::SetNextWindowSizeConstraints({Width, 0}, {Width, Viewport->Size.y * .8f});
	ImGui::SetNextWindowPos(Viewport->GetCenter(), ImGuiCond_Always, {.5f, .5f});
	if (!ImGui::BeginPopupModal(InTitle, &bInOpen,
	                            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
	                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
	{
		return false;
	}
	ImGui::TextWrapped("%s", InMessage.c_str());
	return true;
}

void FGui::EndModal()
{
	Impl->Select();
	ImGui::EndPopup();
}

void FGui::ClosePopup()
{
	Impl->Select();
	ImGui::CloseCurrentPopup();
}

bool FGui::IsEditingText() const
{
	Impl->Select();
	// WantTextInput can retain the previous frame's request after FinishEditing clears the active widget.
	return ImGui::GetIO().WantTextInput && ImGui::IsAnyItemActive();
}

bool FGui::IsTextInputOwnedThisFrame() const
{
	Impl->Select();
	return Impl->bTextInputAtFrameStart || Impl->bClipboardTextUsed || IsEditingText();
}

bool FGui::IsWindowFocused(const char* InTitle) const
{
	Impl->Select();
	const auto* Window = ImGui::FindWindowByName(InTitle);
	if (!Window || !Window->Active || ImGui::GetIO().AppFocusLost)
	{
		return false;
	}
	for (auto* Focused = ImGui::GetCurrentContext()->NavWindow; Focused; Focused = Focused->ParentWindow)
	{
		if (Focused == Window)
		{
			return true;
		}
	}
	return false;
}

bool FGui::HasOpenPopup() const
{
	Impl->Select();
	return ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}
} // namespace Hyperion
