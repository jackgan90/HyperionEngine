#include "EditorApplication.h"

namespace Hyperion
{
void FEditorAcceptanceDriver::CollectInput(FEditorPlugin&, std::vector<FInputEvent>&, std::vector<FInputEvent>&)
{
}

bool FEditorAcceptanceDriver::IsComplete(const FEditorPlugin&) const
{
	return false;
}

bool FEditorAcceptanceDriver::ShouldCapture(const FEditorPlugin& InEditor) const
{
	return !InEditor.Options.Capture.empty() &&
	       ((InEditor.Options.Frames && InEditor.FrameCount + 1 == InEditor.Options.Frames) ||
	        (!InEditor.Options.Benchmark.empty() && InEditor.bBenchmarkTiming &&
	         InEditor.BenchmarkSamples.size() + 1 == InEditor.Options.BenchmarkSamples));
}

void FEditorAcceptanceDriver::CheckCompletion(FEditorPlugin&) const
{
}

void FEditorAcceptanceDriver::CheckTimeout(const FEditorPlugin&, double) const
{
}

void FEditorAcceptanceDriver::CheckGui(FEditorPlugin&, const FGuiDrawData&) const
{
}

void FEditorAcceptanceDriver::CheckAssetWindow(FEditorPlugin&)
{
}

void FEditorAcceptanceDriver::DrawPanels(FEditorPlugin&) const
{
}

EEditorAssetAcceptancePhase FEditorAcceptanceDriver::AssetPhase() const
{
	return EEditorAssetAcceptancePhase::MainWindow;
}

EEditorAcceptanceWindow FEditorAcceptanceDriver::AssetInputWindow() const
{
	return EEditorAcceptanceWindow::Main;
}
} // namespace Hyperion
