#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::EnsureAssetWindow()
{
	if (AssetWindow)
	{
		return;
	}
	auto Layout = Options.Layout;
	Layout += ".assets.ini";
	if (!Acceptance.Policy().bPersistAssetLayout)
	{
		Layout.clear();
	}
	auto Host = std::make_unique<FAssetEditorWindow>(Tasks, *Device, *Compiler, *Session, *AssetWorkspace, Control,
	                                                 std::move(Layout));
	Host->Initialize(*Window, *WindowGroup, IO, Gui->ApplicationScale(), Options.bHidden);
	AssetWindow = std::move(Host);
}

void FEditorPlugin::CloseAssetWindow()
{
	if (PendingImage && PendingImage->Request.Window == "assets")
	{
		PendingImage->Error = "Asset window closed before capture";
		PendingImage.reset();
	}
	AssetWorkspace->CloseAll();
	AssetWindow.reset();
}

bool FEditorPlugin::IsAuxiliaryWindowBlocked() const
{
	return CaptureInteractionPolicy().BlocksAuxiliaryWindows();
}

void FEditorPlugin::SynchronizeWindowGroup()
{
	WindowGroup->SetModalActive(IsAuxiliaryWindowBlocked());
	WindowGroup->Synchronize();
}

void FEditorPlugin::AdvanceAssetWindow(float InDelta, std::vector<FInputEvent> InEvents,
                                       const std::filesystem::path& InCapture)
{
	if (!AssetWindow && AssetWorkspace->HasDocuments())
	{
		EnsureAssetWindow();
	}
	if (AssetWindow)
	{
		const bool bMainDrawable = !Window->Minimized() && Window->PixelSize().Width && Window->PixelSize().Height;
		AssetWindow->Advance(InDelta, std::move(InEvents), Gui->ApplicationScale(), IsAuxiliaryWindowBlocked(),
		                     GetDepthConvention(Rendering.bReversedZ), InCapture,
		                     !bMainDrawable && Acceptance.Policy().bAllowAssetWindowWait && Options.Benchmark.empty());
		if (PendingImage && (PendingImage->Result || !PendingImage->Error.empty()))
		{
			PendingImage.reset();
		}
		Acceptance.CheckAssetWindow();
	}
}
} // namespace Hyperion
