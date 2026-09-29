#include "GuiInternal.h"
#include <algorithm>
#include <climits>

namespace Hyperion
{
void FGui::TextRows(const char* InId, std::uint64_t InCount,
                    const std::function<std::vector<FGuiTextRow>(std::uint64_t, std::uint32_t)>& InRead)
{
	Impl->Select();
	if (ImGui::BeginChild(InId, {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
	{
		const bool bAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1;
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
		const float Height = ImGui::GetTextLineHeight();
		try
		{
			ImGuiListClipper Clipper;
			Clipper.Begin(static_cast<int>(std::min<std::uint64_t>(InCount, INT_MAX)), Height);
			while (Clipper.Step())
			{
				const auto Rows = InRead(Clipper.DisplayStart, Clipper.DisplayEnd - Clipper.DisplayStart);
				for (const auto& Row : Rows)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, {Row.Color.X, Row.Color.Y, Row.Color.Z, Row.Color.W});
					ImGui::TextUnformatted(Row.Text.data(), Row.Text.data() + Row.Text.size());
					ImGui::PopStyleColor();
				}
			}
			if (bAtBottom)
			{
				ImGui::SetScrollY(ImGui::GetCursorPosY());
			}
		}
		catch (...)
		{
			ImGui::PopStyleVar();
			ImGui::EndChild();
			throw;
		}
		ImGui::PopStyleVar();
	}
	ImGui::EndChild();
}
} // namespace Hyperion
