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
	if (!Options.ExerciseAssets.empty() || !Options.ExerciseContent.empty())
	{
		Layout.clear();
	}
	auto Host = std::make_unique<FAssetEditorWindow>(Tasks, *Device, *Compiler, *Session, *AssetWorkspace, Control,
	                                                 std::move(Layout));
	Host->Initialize(*Window, IO, Gui->ApplicationScale(), Options.bHidden);
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

bool FEditorPlugin::IsAssetWindowBlocked() const
{
	return PendingRoot.has_value() || bDiscardDialog || bSaveDialog || bOpenDialog || bAssetMessage ||
	       bPreferencesDialog || bSaveThenClose;
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
		AssetWindow->Advance(InDelta, std::move(InEvents), Gui->ApplicationScale(), IsAssetWindowBlocked(), InCapture,
		                     !bMainDrawable && Options.ExerciseAssets.empty() && Options.Benchmark.empty());
		if (PendingImage && (PendingImage->Result || !PendingImage->Error.empty()))
		{
			PendingImage.reset();
		}
		if (!Options.ExerciseAssets.empty() && (ExerciseStep == 132 || ExerciseStep == 133) &&
		    AssetWorkspace->HasPendingEdits() && !bPendingAssetEditChecked)
		{
			CheckPendingAssetEdit();
		}
	}
}
} // namespace Hyperion
