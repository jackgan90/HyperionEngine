#include "EditorApplication.h"
#include "EditorTextFilter.h"
#include <array>

namespace Hyperion
{
namespace
{
std::string KindName(ESceneNodeKind InKind)
{
	constexpr std::array Names{"Folder",    "Model",       "Camera",    "Directional Light",
	                           "Sky Light", "Point Light", "Spot Light"};
	return Names.at(static_cast<std::size_t>(InKind));
}

} // namespace

void FEditorPlugin::DrawNode(FSceneHandle InHandle)
{
	struct FNodeVisit
	{
		FSceneHandle Handle;
		bool bEndTree{};
	};

	std::vector<FNodeVisit> Pending{{InHandle}};
	while (!Pending.empty())
	{
		const FNodeVisit Visit = Pending.back();
		Pending.pop_back();
		if (Visit.bEndTree)
		{
			Gui->EndTree();
			continue;
		}
		const auto* Node = Scene->FindNode(Visit.Handle);
		if (!Node)
		{
			continue;
		}
		const auto Children = Scene->GetChildren(Visit.Handle);
		OutlinerRows.push_back(Visit.Handle);
		Gui->NextRow();
		Gui->NextColumn();
		bool bClicked{};
		if (Reparent.ConsumeExpansion(Node->Id))
		{
			Gui->OpenNextTreeItem();
		}
		const bool bOpen = Gui->TreeItem(Node->Id.c_str(), Node->Name.c_str(), Children.empty(),
		                                 Selection.Contains(Visit.Handle), bClicked, !Options.bBenchmarkCollapsed);
		Acceptance.ObserveWidget(EEditorWidget::OutlinerTreeItem, Gui->LastItemBounds(), Node->Id);
		RouteReparentRow(Visit.Handle);
		Acceptance.ObserveWidget(EEditorWidget::OutlinerRow, Gui->LastItemBounds(), Node->Id);
		Gui->NextColumn();
		Gui->Text(KindName(Node->GetKind()));
		if (bOpen)
		{
			// Keep the parent tree scope open until its children have been visited in order.
			Pending.push_back({Visit.Handle, true});
			for (auto Child = Children.rbegin(); Child != Children.rend(); ++Child)
			{
				Pending.push_back({*Child});
			}
		}
	}
}

void FEditorPlugin::DrawOutliner()
{
	OutlinerRows.clear();
	if (!bShowOutliner)
	{
		return;
	}
	if (Gui->BeginWindow("Outliner", bShowOutliner))
	{
		if (!bSelectionInitialized && !Selection && Scene->GetStatus().bReady)
		{
			bSelectionInitialized = true;
			const auto Models = Scene->GetNodes(ESceneNodeKind::Model);
			if (!Models.empty())
			{
				Selection = Models.front();
			}
		}
		Gui->Text("Search objects");
		Gui->SetNextItemWidth(-1);
		Gui->InputText("##SearchObjects", Filter, false);
		if (OutlinerSelectionFilter != Filter)
		{
			OutlinerSelectionFilter = Filter;
			OutlinerSelection.Reset();
			CancelReparentGesture();
		}
		Acceptance.ObserveWidget(EEditorWidget::OutlinerSearch, Gui->LastItemBounds());
		Gui->Text(std::to_string(Scene->GetStatus().Nodes) + " objects" +
		          ("  |  " + std::to_string(Selection.All().size()) + " selected"));
		DrawReparentRoot();
		if (Gui->BeginTable("Objects", "Item Label", "Type"))
		{
			if (Filter.empty())
			{
				for (const auto Root : Scene->GetRoots())
				{
					DrawNode(Root);
				}
			}
			else
			{
				for (const auto Handle : Scene->GetNodes())
				{
					const auto* Node = Scene->FindNode(Handle);
					if (!Node || !MatchesEditorFilter(Node->Name, Filter))
					{
						continue;
					}
					Gui->NextRow();
					Gui->NextColumn();
					OutlinerRows.push_back(Handle);
					const bool bActivated =
					    Gui->Selectable((Node->Name + "##" + Node->Id).c_str(), Selection.Contains(Handle));
					RouteReparentRow(Handle, bActivated);
					Acceptance.ObserveWidget(EEditorWidget::OutlinerRow, Gui->LastItemBounds(), Node->Id);
					Gui->NextColumn();
					Gui->Text(KindName(Node->GetKind()));
				}
			}
			Gui->ScrollDragTarget();
			Gui->EndTable();
		}
	}
	Gui->EndWindow();
}

} // namespace Hyperion
