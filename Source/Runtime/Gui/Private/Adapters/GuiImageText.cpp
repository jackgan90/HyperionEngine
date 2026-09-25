#include "GuiInternal.h"
#include <algorithm>

namespace Hyperion
{
FVec4 FGui::DrawImageText(FVec4 InClip, std::span<const std::string> InLines, bool bInRightAligned)
{
	Impl->Select();
	const float Step = std::min(ImGui::GetFontSize(), std::max(14.f, (InClip.Z - InClip.X) / 12));
	const float Padding = std::min(8 * Impl->AppliedScale, Step * .5f);
	const float Spacing = std::min(ImGui::GetStyle().ItemSpacing.y, Step * .2f);
	auto* Font = ImGui::GetFont();
	const float Available = InClip.W - InClip.Y - 4 * Padding;
	if (InLines.empty() || Available < Step || InClip.Z - InClip.X < 4 * Padding)
	{
		return {};
	}
	const float MaximumWidth = InClip.Z - InClip.X - 4 * Padding;
	std::vector<std::string> Lines;
	std::vector<float> Heights;
	float Width{};
	float Height{};
	for (const auto& Line : InLines)
	{
		const auto Size = Font->CalcTextSizeA(Step, FLT_MAX, MaximumWidth, Line.c_str());
		if (Height + Size.y > Available)
		{
			const std::string Overflow = "...";
			while (!Lines.empty() && Height + Step > Available)
			{
				Height -= Heights.back() + Spacing;
				Lines.pop_back();
				Heights.pop_back();
			}
			Lines.push_back(Overflow);
			Heights.push_back(Step);
			Height += Step + Spacing;
			Width = std::max(Width, Font->CalcTextSizeA(Step, FLT_MAX, MaximumWidth, Overflow.c_str()).x);
			break;
		}
		Lines.push_back(Line);
		Heights.push_back(Size.y);
		Width = std::max(Width, Size.x);
		Height += Size.y + Spacing;
	}
	Width = std::min(Width, MaximumWidth);
	Height -= Spacing;
	const float Left = bInRightAligned ? InClip.Z - Width - 3 * Padding : InClip.X + Padding;
	const float Top = InClip.Y + Padding;
	const FVec4 Bounds{Left, Top, Left + Width + 2 * Padding, Top + Height + 2 * Padding};
	auto* Draw = ImGui::GetWindowDrawList();
	Draw->PushClipRect({InClip.X, InClip.Y}, {InClip.Z, InClip.W}, true);
	Draw->AddRectFilled({Bounds.X, Bounds.Y}, {Bounds.Z, Bounds.W}, IM_COL32(10, 14, 20, 190), 4 * Impl->AppliedScale);
	Draw->PushClipRect({Left + Padding, Top + Padding}, {Left + Width + Padding, Bounds.W - Padding}, true);
	float Y = Top + Padding;
	for (std::size_t Index = 0; Index < Lines.size(); ++Index)
	{
		Draw->AddText(Font, Step, {Left + Padding, Y}, IM_COL32(235, 240, 247, 255), Lines[Index].c_str(), nullptr,
		              MaximumWidth);
		Y += Heights[Index] + Spacing;
	}
	Draw->PopClipRect();
	Draw->PopClipRect();
	return Bounds;
}
} // namespace Hyperion
