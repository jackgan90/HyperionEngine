#include "EditorApplication.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
void FEditorPlugin::RefreshContent()
{
	if (!Browser)
	{
		return;
	}
	ScenePaths.clear();
	CatalogError.clear();
	Browser->Refresh(!Context.Require<FContentRootService>().Directory().empty(), bShowInternalAssets);
}

void FEditorPlugin::PollContent()
{
	if (auto Result = Browser->PollScenes())
	{
		ScenePaths = std::move(Result->Paths);
		CatalogError = std::move(Result->Error);
		if (OpenPath.empty() && !ScenePaths.empty())
		{
			OpenPath = ScenePaths.front();
		}
	}
	if (PendingAssetOpen && PendingAssetOpen->Ready())
	{
		try
		{
			const auto Header = PendingAssetOpen->GetReady();
			if (Header->TypeId == "hyperion.scene")
			{
				OpenScene(AssetOpenPath);
			}
			else
			{
				AssetMessage =
				    "Opening this asset type is not supported yet.\nType: " + Header->TypeId + "\n" + AssetOpenPath;
				bAssetMessage = bRequestAssetMessage = true;
			}
		}
		catch (const std::exception& Failure)
		{
			AssetMessage = "Could not read asset: " + AssetOpenPath + "\n" + Failure.what();
			bAssetMessage = bRequestAssetMessage = true;
		}
		PendingAssetOpen.reset();
	}
}

void FEditorPlugin::RequestOpenAsset(const std::string& InPath)
{
	if (PendingAssetOpen || PendingRoot)
	{
		return;
	}
	AssetOpenPath = InPath;
	AssetOpenCancellation = {};
	PendingAssetOpen = DispatchAsync<FAssetHeader>(
	    Tasks, {EDomain::Worker},
	    [Service = &IO, Path = PathFromUtf8(InPath), Token = AssetOpenCancellation]
	    {
		    return ReadContentHeader(*Service, Path, Token);
	    },
	    AssetOpenCancellation);
}

void FEditorPlugin::DrawAssetMessage()
{
	if (bRequestAssetMessage)
	{
		Gui->OpenPopup("Open Asset");
		bRequestAssetMessage = false;
	}
	if (bAssetMessage && Gui->BeginModal("Open Asset", bAssetMessage))
	{
		Gui->TextWrapped(AssetMessage);
		if (Gui->Button("OK"))
		{
			bAssetMessage = false;
			Gui->ClosePopup();
		}
		Gui->EndModal();
	}
}

void FEditorPlugin::DrawContentTree(const std::string& InPath)
{
	const auto* Directory = Browser->Directory(InPath);
	const bool bAncestor = Browser->SelectedDirectory == InPath || Browser->SelectedDirectory.starts_with(InPath + "/");
	if (bRevealContentTree && bAncestor)
	{
		Gui->SetNextTreeOpen(true);
	}
	bool bClicked{};
	const auto Label = InPath == "/Game" ? "All" : PathToUtf8(PathFromUtf8(InPath).filename());
	const bool bOpen = Gui->TreeItem(InPath.c_str(), Label.c_str(), false, Browser->SelectedDirectory == InPath,
	                                 bClicked, InPath == "/Game");
	if (bClicked)
	{
		Browser->Navigate(InPath);
	}
	if (bOpen)
	{
		if (Directory)
		{
			for (const auto& Entry : Directory->Entries)
			{
				if (Entry.bDirectory)
				{
					DrawContentTree(PathToUtf8(Entry.Path));
				}
			}
		}
		Gui->EndTree();
	}
}

void FEditorPlugin::DrawContentGrid()
{
	const auto* Directory = Browser->Directory(Browser->SelectedDirectory);
	if (!Directory)
	{
		Gui->Text("Loading directory...");
		return;
	}
	if (!Directory->Error.empty())
	{
		Gui->TextWrapped(Directory->Error);
	}
	if (Directory->Entries.empty())
	{
		Gui->Text("This folder contains no visible assets or folders.");
	}
	if (!Gui->BeginTileGrid("ContentTiles"))
	{
		return;
	}
	for (const auto& Entry : Directory->Entries)
	{
		Gui->NextColumn();
		const auto Path = PathToUtf8(Entry.Path);
		const auto Name = PathToUtf8(Entry.Path.filename());
		bool bDoubleClicked{};
		if (Gui->FileTile(Path.c_str(), Name.c_str(), Entry.bDirectory, Browser->SelectedFile == Path, bDoubleClicked))
		{
			Browser->SelectedFile = Path;
			if (bDoubleClicked && Entry.bDirectory)
			{
				Browser->Navigate(Path);
				bRevealContentTree = true;
			}
			else if (bDoubleClicked)
			{
				RequestOpenAsset(Path);
			}
		}
		if (!Options.ExerciseContent.empty())
		{
			ContentTileBounds[Path] = Gui->LastItemBounds();
		}
	}
	Gui->EndTable();
}

void FEditorPlugin::DrawSceneBrowser()
{
	ContentTileBounds.clear();
	if (!bShowBrowser)
	{
		return;
	}
	if (Gui->BeginWindow("Content Browser", bShowBrowser))
	{
		const auto Root = Context.Require<FContentRootService>().Directory();
		if (Gui->Button("Refresh"))
		{
			RefreshContent();
		}
		Gui->SameLine();
		if (Gui->Checkbox("Show Internal Assets", bShowInternalAssets))
		{
			Browser->SelectedDirectory = "/Game";
			Browser->SelectedFile.clear();
			RefreshContent();
		}
		InternalAssetsBounds = Gui->LastItemBounds();
		if (Root.empty())
		{
			Gui->TextWrapped("Choose an asset root with File > Open...");
		}
		else
		{
			const auto RelativePath = ContentRelativePath(Browser->SelectedDirectory);
			Gui->Text(RelativePath.empty() ? "All" : "All/" + RelativePath);
			Gui->Tooltip(PathToUtf8(Root).c_str());
			if (Gui->BeginSplitPane("ContentSplit"))
			{
				Gui->NextColumn();
				Gui->BeginScrollRegion("DirectoryTree", 0);
				DrawContentTree("/Game");
				bRevealContentTree = false;
				Gui->EndScrollRegion();
				Gui->NextColumn();
				Gui->BeginScrollRegion("DirectoryContents", 0);
				DrawContentGrid();
				Gui->EndScrollRegion();
				Gui->EndTable();
			}
		}
	}
	Gui->EndWindow();
}
} // namespace Hyperion
