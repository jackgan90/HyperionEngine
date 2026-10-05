#include "../../Private/EditorAcceptanceReport.h"
#include "EditorAcceptanceHarness.h"

namespace Hyperion
{
FEditorAcceptanceDriver::FEditorAcceptanceDriver() = default;
FEditorAcceptanceDriver::~FEditorAcceptanceDriver() = default;

void FEditorAcceptanceDriver::Initialize(FEditorPlugin& InEditor)
{
	Harness = FEditorAcceptanceHarness::Create(InEditor);
}

void FEditorAcceptanceDriver::CollectInput(std::vector<FInputEvent>& InEvents, std::vector<FInputEvent>& InAssetEvents)
{
	if (Harness)
	{
		Harness->CollectInput(InEvents, InAssetEvents);
	}
}

bool FEditorAcceptanceDriver::IsComplete() const
{
	return Harness ? Harness->IsComplete() : false;
}

bool FEditorAcceptanceDriver::ShouldCapture() const
{
	return Harness ? Harness->ShouldCapture() : false;
}

void FEditorAcceptanceDriver::CheckCompletion() const
{
	if (Harness)
	{
		Harness->CheckCompletion();
	}
}

void FEditorAcceptanceDriver::CheckTimeout(double InElapsed) const
{
	if (Harness)
	{
		Harness->CheckTimeout(InElapsed);
	}
}

void FEditorAcceptanceDriver::CheckGui(const FGuiDrawData& InData)
{
	if (Harness)
	{
		Harness->CheckGui(InData);
	}
}

void FEditorAcceptanceDriver::CheckAssetWindow()
{
	if (Harness)
	{
		Harness->CheckAssetWindowFrame();
	}
}

void FEditorAcceptanceDriver::DrawPanels() const
{
	if (Harness)
	{
		Harness->DrawPanels();
	}
}

void FEditorAcceptanceDriver::BeginSurface(EEditorSurface InSurface)
{
	if (Harness)
	{
		Harness->BeginSurface(InSurface);
	}
}

void FEditorAcceptanceDriver::ObserveWidget(EEditorWidget InWidget, FVec4 InBounds, std::string_view InId)
{
	if (Harness)
	{
		Harness->ObserveWidget(InWidget, InBounds, InId);
	}
}

void FEditorAcceptanceDriver::ObserveIndexedWidget(EEditorWidget InWidget, FVec4 InBounds, std::size_t InIndex)
{
	if (Harness)
	{
		Harness->ObserveIndexedWidget(InWidget, InBounds, InIndex);
	}
}

std::function<void(std::string_view, FVec4)> FEditorAcceptanceDriver::PropertyObserver(std::string_view InComponent,
                                                                                       bool bInSelection)
{
	return Harness ? Harness->PropertyObserver(InComponent, bInSelection)
	               : std::function<void(std::string_view, FVec4)>{};
}

void FEditorAcceptanceDriver::ObservePlacement(const std::shared_ptr<const FTransientGeometry>& InPreview) const
{
	if (Harness)
	{
		Harness->ObservePlacement(InPreview);
	}
}

std::span<const FSceneHandle> FEditorAcceptanceDriver::OutlineSelection(std::span<const FSceneHandle> InSelection) const
{
	return Harness ? Harness->OutlineSelection(InSelection) : InSelection;
}

std::filesystem::path FEditorAcceptanceDriver::MainCapture() const
{
	return Harness ? Harness->MainCapture() : std::filesystem::path{};
}

std::filesystem::path FEditorAcceptanceDriver::TakeAssetCapture()
{
	return Harness ? Harness->TakeAssetCapture() : std::filesystem::path{};
}

void FEditorAcceptanceDriver::CompleteCapture(const FImage& InImage, const std::filesystem::path& InPath)
{
	if (Harness)
	{
		Harness->CompleteCapture(InImage, InPath);
	}
}

bool FEditorAcceptanceDriver::RevealContent(std::string_view InPath)
{
	return Harness ? Harness->RevealContent(InPath) : false;
}

void FEditorAcceptanceDriver::WriteReport(std::ostream& InStream) const
{
	if (Harness)
	{
		Harness->WriteReport(InStream);
	}
	else
	{
		WriteEditorAcceptanceReport(InStream, {});
	}
}

const FEditorAcceptancePolicy& FEditorAcceptanceDriver::Policy() const
{
	static const FEditorAcceptancePolicy Default;
	return Harness ? Harness->Policy() : Default;
}
} // namespace Hyperion
