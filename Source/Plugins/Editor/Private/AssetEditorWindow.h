#pragma once
#include "AssetWorkspace.h"
#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/RHI/RHISwapchain.h"

namespace Hyperion
{
// Main-owned secondary host. The Editor retains document and shared-service ownership.
class FAssetEditorWindow
{
public:
	FAssetEditorWindow(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
	                   FRenderSession& InSession, FAssetWorkspace& InWorkspace, FApplicationControl& InControl,
	                   std::filesystem::path InLayout);
	~FAssetEditorWindow();
	void Initialize(FWindow& InOwner, FIOService& InIO, float InScale, bool bInHidden);
	void Activate();
	void Poll(bool bInBlocked);
	bool ShouldClose() const;
	bool IsDrawable() const;
	void Advance(float InDelta, std::vector<FInputEvent> InEvents, float InScale, bool bInBlocked,
	             const std::filesystem::path& InCapture = {}, bool bInVsync = false);
	FWindow& NativeWindow();
	FGui& GuiContext();
	FVec4 ObservedBounds(std::string_view InId) const;
	std::uint64_t RenderedFrames() const;

private:
	void Stop();
	void SaveLayout();
	void RouteShortcuts(std::vector<FInputEvent>& InEvents);
	FGuiDrawData Draw(float InDelta, std::span<const FInputEvent> InEvents, bool bInBlocked);
	void DrawMenus();
	void DrawCloseDialog();
	void Render(FGuiDrawData InData, const std::filesystem::path& InCapture, bool bInVsync);
	FTaskSystem& Tasks;
	IRHIDevice& Device;
	FShaderCompiler& Compiler;
	FRenderSession& Session;
	FAssetWorkspace& Workspace;
	FApplicationControl& Control;
	std::filesystem::path Layout;
	FWindow* Owner{};
	std::unique_ptr<FWindow> Window;
	std::unique_ptr<FGui> Gui;
	std::unique_ptr<FGuiRenderer> Renderer;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::map<std::string, FVec4, std::less<>> Bounds;
	std::string Error;
	std::uint64_t FrameCount{};
	bool bHidden{};
	bool bClosePending{};
	bool bCloseDialog{};
	bool bRequestClose{};
	bool bSaveThenClose{};
	bool bShowPreview = true;
	bool bShowProperties = true;
	bool bResetLayout{};
};
} // namespace Hyperion
