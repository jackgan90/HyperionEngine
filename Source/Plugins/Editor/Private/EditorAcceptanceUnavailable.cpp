#include "EditorAcceptanceDriver.h"
#include "EditorAcceptanceReport.h"

namespace Hyperion
{
class FEditorAcceptanceHarness
{
};

FEditorAcceptanceDriver::FEditorAcceptanceDriver() = default;
FEditorAcceptanceDriver::~FEditorAcceptanceDriver() = default;

void FEditorAcceptanceDriver::Initialize(FEditorPlugin&)
{
}

void FEditorAcceptanceDriver::CollectInput(std::vector<FInputEvent>&, std::vector<FInputEvent>&)
{
}

bool FEditorAcceptanceDriver::IsComplete() const
{
	return false;
}

bool FEditorAcceptanceDriver::ShouldCapture() const
{
	return false;
}

void FEditorAcceptanceDriver::CheckCompletion() const
{
}

void FEditorAcceptanceDriver::CheckTimeout(double) const
{
}

void FEditorAcceptanceDriver::CheckGui(const FGuiDrawData&)
{
}

void FEditorAcceptanceDriver::CheckAssetWindow()
{
}

void FEditorAcceptanceDriver::DrawPanels() const
{
}

void FEditorAcceptanceDriver::BeginSurface(EEditorSurface)
{
}

void FEditorAcceptanceDriver::ObserveWidget(EEditorWidget, FVec4, std::string_view)
{
}

void FEditorAcceptanceDriver::ObserveIndexedWidget(EEditorWidget, FVec4, std::size_t)
{
}

std::function<void(std::string_view, FVec4)> FEditorAcceptanceDriver::PropertyObserver(std::string_view, bool)
{
	return std::function<void(std::string_view, FVec4)>{};
}

void FEditorAcceptanceDriver::ObservePlacement(const std::shared_ptr<const FTransientGeometry>&) const
{
}

std::span<const FSceneHandle> FEditorAcceptanceDriver::OutlineSelection(std::span<const FSceneHandle> InSelection) const
{
	return InSelection;
}

std::filesystem::path FEditorAcceptanceDriver::MainCapture() const
{
	return std::filesystem::path{};
}

std::filesystem::path FEditorAcceptanceDriver::TakeAssetCapture()
{
	return std::filesystem::path{};
}

void FEditorAcceptanceDriver::CompleteCapture(const FImage&, const std::filesystem::path&)
{
}

bool FEditorAcceptanceDriver::RevealContent(std::string_view)
{
	return false;
}

void FEditorAcceptanceDriver::WriteReport(std::ostream& InStream) const
{
	WriteEditorAcceptanceReport(InStream, {});
}

const FEditorAcceptancePolicy& FEditorAcceptanceDriver::Policy() const
{
	static const FEditorAcceptancePolicy Default;
	return Default;
}
} // namespace Hyperion
