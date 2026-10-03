#pragma once
#include "EditorObservations.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Scene/Scene.h"
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>

namespace Hyperion
{
class FEditorPlugin;
class FEditorAcceptanceHarness;
struct FTransientGeometry;
struct FImage;

// Existing acceptance execution choices; normal Editor runs retain these defaults.
struct FEditorAcceptancePolicy
{
	std::optional<float> GuiDelta;
	std::optional<float> CameraDelta;
	bool bUseVsync = true;
	bool bUseFrameLimit = true;
	bool bAllowAssetWindowWait = true;
	bool bPersistAssetLayout = true;
};

// Main-only boundary. Scenario state and methods are defined exclusively by the test implementation.
class FEditorAcceptanceDriver
{
public:
	FEditorAcceptanceDriver();
	~FEditorAcceptanceDriver();
	void Initialize(FEditorPlugin& InEditor);
	void CollectInput(std::vector<FInputEvent>& InEvents, std::vector<FInputEvent>& InAssetEvents);
	bool IsComplete() const;
	bool ShouldCapture() const;
	void CheckCompletion() const;
	void CheckTimeout(double InElapsed) const;
	void CheckGui(const FGuiDrawData& InData);
	void CheckAssetWindow();
	void DrawPanels() const;
	void BeginSurface(EEditorSurface InSurface);
	void ObserveWidget(EEditorWidget InWidget, FVec4 InBounds, std::string_view InId = {});
	void ObserveIndexedWidget(EEditorWidget InWidget, FVec4 InBounds, std::size_t InIndex);
	std::function<void(std::string_view, FVec4)> PropertyObserver(std::string_view InComponent, bool bInSelection);
	void ObservePlacement(const std::shared_ptr<const FTransientGeometry>& InPreview) const;
	std::span<const FSceneHandle> OutlineSelection(std::span<const FSceneHandle> InSelection) const;
	std::filesystem::path MainCapture() const;
	std::filesystem::path TakeAssetCapture();
	void CompleteCapture(const FImage& InImage, const std::filesystem::path& InPath);
	bool RevealContent(std::string_view InPath);
	void WriteReport(std::ostream& InStream) const;
	const FEditorAcceptancePolicy& Policy() const;

private:
	std::unique_ptr<FEditorAcceptanceHarness> Harness;
};
} // namespace Hyperion
