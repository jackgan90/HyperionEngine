#include "GuiInternal.h"
#include <cmath>

namespace Hyperion
{
void FGui::SetApplicationScale(float InScale)
{
	Impl->Select();
	if (!std::isfinite(InScale) || InScale < 1 || InScale > 2)
	{
		throw std::invalid_argument("GUI application scale must be finite and between 1 and 2");
	}
	Impl->RequestedScale = InScale;
}

float FGui::ApplicationScale() const
{
	Impl->Select();
	return Impl->RequestedScale;
}

bool FGui::ApplicationScaleControl(const char* InLabel)
{
	Impl->Select();
	float Value = Impl->RequestedScale;
	const float Minimum = 1;
	const float Maximum = 2;
	if (!Impl->DragNumber(InLabel, ImGuiDataType_Float, &Value, .01f, "%.2f", &Minimum, &Maximum) ||
	    !std::isfinite(Value))
	{
		return false;
	}
	SetApplicationScale(Value);
	return true;
}

float FGui::Scale(float InSize) const
{
	Impl->Select();
	return InSize * Impl->AppliedScale;
}

void FGui::FImpl::RebuildFont()
{
	auto& Io = ImGui::GetIO();
	Io.FontDefault = nullptr;
	Io.Fonts->Clear();
	FontAtlas.reset();
	ImFontConfig Configuration;
	Configuration.SizePixels = FontPixels * AppliedScale * FontDensity;
	Configuration.FontDataOwnedByAtlas = false;
	if (FontBytes.empty())
	{
		Io.Fonts->AddFontDefaultVector(&Configuration);
	}
	else if (!Io.Fonts->AddFontFromMemoryTTF(FontBytes.data(), static_cast<int>(FontBytes.size()),
	                                         Configuration.SizePixels, &Configuration))
	{
		throw std::runtime_error("Could not load GUI font");
	}
	// Atlas replacement can preserve the context's previous base size. Set it
	// explicitly so the new raster size and the next frame's layout agree.
	ImGui::GetStyle().FontSizeBase = Configuration.SizePixels;
}

void FGui::FImpl::ApplyScale(float InDensity)
{
	if (AppliedScale == RequestedScale && FontDensity == InDensity)
	{
		return;
	}
	AppliedScale = RequestedScale;
	FontDensity = InDensity;
	auto& Style = ImGui::GetStyle();
	Style = BaseStyle;
	Style.ScaleAllSizes(AppliedScale);
	Style.FontScaleMain = 1 / FontDensity;
	auto& PlotStyle = ImPlot::GetStyle();
	PlotStyle = BasePlotStyle;
	for (auto* Metric :
	     {&PlotStyle.PlotDefaultSize, &PlotStyle.PlotMinSize, &PlotStyle.MajorTickLen, &PlotStyle.MinorTickLen,
	      &PlotStyle.MajorTickSize, &PlotStyle.MinorTickSize, &PlotStyle.MajorGridSize, &PlotStyle.MinorGridSize,
	      &PlotStyle.PlotPadding, &PlotStyle.LabelPadding, &PlotStyle.LegendPadding, &PlotStyle.LegendInnerPadding,
	      &PlotStyle.LegendSpacing, &PlotStyle.MousePosPadding, &PlotStyle.AnnotationPadding})
	{
		Metric->x *= AppliedScale;
		Metric->y *= AppliedScale;
	}
	PlotStyle.PlotBorderSize *= AppliedScale;
	PlotStyle.DigitalPadding *= AppliedScale;
	PlotStyle.DigitalSpacing *= AppliedScale;
	RebuildFont();
}
} // namespace Hyperion
