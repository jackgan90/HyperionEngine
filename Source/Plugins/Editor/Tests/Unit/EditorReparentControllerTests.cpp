#include "../../Private/EditorReparentController.h"
#include "../../Private/OutlinerScene.h"
#include "../Acceptance/AcceptanceBounds.h"
#include "Hyperion/SceneEditing/SceneAuthoring.h"
#include "Support/TestSupport.h"
#include <iostream>

using namespace Hyperion;

namespace
{
void CheckAcceptanceBoundsIdentities()
{
	FAcceptanceBounds Bounds;
	std::string TemporaryId = "1";
	const auto Item = FWidgetKey::WithId(EEditorWidget::PlacementItem, TemporaryId);
	Bounds.Set(Item, {1, 2, 3, 4});
	TemporaryId = "changed";
	Bounds.Set(FWidgetKey::WithId(EEditorWidget::RenderPipelineItem, "1"), {5, 6, 7, 8});
	Bounds.Set(FWidgetKey::WithValue(EEditorWidget::PlacementItem, 1), {9, 10, 11, 12});
	Bounds.Set(FPropertyKey{"component/a", "field"}, {13, 14, 15, 16});
	Bounds.Set(FPropertyKey{"component", "a/field"}, {17, 18, 19, 20});
	HYP_CHECK(Bounds.Require(FWidgetKey::WithId(EEditorWidget::PlacementItem, "1")).X == 1);
	HYP_CHECK(Bounds.Require(FWidgetKey::WithId(EEditorWidget::RenderPipelineItem, "1")).X == 5);
	HYP_CHECK(Bounds.Require(FWidgetKey::WithValue(EEditorWidget::PlacementItem, 1)).X == 9);
	HYP_CHECK(Bounds.Require(FPropertyKey{"component/a", "field"}).X == 13);
	HYP_CHECK(Bounds.Require(FPropertyKey{"component", "a/field"}).X == 17);
	HYP_CHECK(Bounds.FindOrEmpty(EEditorWidget::HierarchyRoot).Z == 0);
	HYP_CHECK(!Bounds.Contains(EEditorWidget::HierarchyRoot));
	bool bMissingRejected{};
	try
	{
		Bounds.Require(EEditorWidget::HierarchyRoot);
	}
	catch (const std::out_of_range&)
	{
		bMissingRejected = true;
	}
	HYP_CHECK(bMissingRejected);
	Bounds.Clear();
	HYP_CHECK(!Bounds.Contains(Item) && !Bounds.Contains(FPropertyKey{"component/a", "field"}));
}

// A real CPU scene; unsupported fixture operations fail instead of simulating persistence or rendering.
class FTestSceneTarget final : public ISceneEditTarget
{
public:
	bool IsLoaded() const override
	{
		return true;
	}

	bool IsReady() const override
	{
		return true;
	}

	std::uint64_t Identity() const override
	{
		return Scene.GetIdentity();
	}

	std::uint64_t Revision() const override
	{
		return Scene.GetRevision();
	}

	const FSceneNode* FindNode(FSceneHandle InHandle) const override
	{
		return Scene.FindNode(InHandle);
	}

	FSceneHandle FindHandle(std::string_view InId) const override
	{
		return Scene.FindHandle(InId);
	}

	std::vector<FSceneHandle> Nodes() const override
	{
		return Scene.GetNodes();
	}

	std::vector<FSceneHandle> Children(FSceneHandle InHandle) const override
	{
		return Scene.GetChildren(InHandle);
	}

	bool NodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const override
	{
		return Scene.GetNodeView(InHandle, OutView);
	}

	const FSceneSettings& Settings() const override
	{
		return Scene.GetSettings();
	}

	bool EditNodes(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision) override
	{
		return Scene.EditNodes(std::move(InEdits), InExpectedRevision);
	}

	FSceneHandle AddNode(FSceneNode InNode) override
	{
		return Scene.AddNode(std::move(InNode));
	}

	FSceneHandle DuplicateNode(FSceneHandle) override
	{
		throw std::logic_error("Unexpected fixture duplication");
	}

	bool RemoveNodeKeepChildren(FSceneHandle InHandle) override
	{
		return Scene.RemoveNodeKeepChildren(InHandle);
	}

	std::vector<FSceneHandle> AddNodes(std::vector<FSceneNode> InNodes,
	                                   std::vector<FSceneNodeEdit> InRestoredChildren) override
	{
		return Scene.AddNodes(std::move(InNodes), std::move(InRestoredChildren));
	}

	bool RemoveSubtrees(std::span<const FSceneHandle> InRoots) override
	{
		return Scene.RemoveSubtrees(InRoots);
	}

	void SetSettings(FSceneSettings InSettings) override
	{
		Scene.SetSettings(std::move(InSettings));
	}

	FSceneNode Rebind(FSceneNode InNode) override
	{
		return InNode;
	}

	void RefreshAssets() override
	{
		throw std::logic_error("Unexpected fixture asset refresh");
	}

	TAsyncResult<bool> Save(const std::filesystem::path&) override
	{
		throw std::logic_error("Persistence is covered by Editor acceptance");
	}

	FScene Scene;
};

FVec2 Center(FVec4 InBounds)
{
	return {(InBounds.X + InBounds.Z) * .5f, (InBounds.Y + InBounds.W) * .5f};
}

struct FReparentFixture final : IEditorReparentActions
{
	FTaskSystem Tasks{1, 1};
	FTestSceneTarget Target;
	FSceneEditDocument Document;
	FGui Gui;
	FEditorReparentController Controller;
	FSceneHandle Source;
	FSceneHandle Parent;
	FVec4 SourceBounds;
	FVec4 ParentBounds;
	FVec4 RootBounds;
	std::optional<std::string> Error;
	unsigned Clicks{};
	unsigned DragStarts{};
	bool bResetBeforeFinish{};
	bool bSceneOpen{};
	bool bExpandScene{};

	FReparentFixture()
	{
		FSceneNode Node;
		Node.Id = "source";
		Node.Name = "Source";
		Source = Target.AddNode(Node);
		Node.Id = "parent";
		Node.Name = "Parent";
		Parent = Target.AddNode(Node);
		Document.Attach(Target);
		Document.ReplaceSelection(FSceneSelection(Source));
		Gui.FontImage();
		Frame();
		Frame();
	}

	void FinishInspectorEdit() override
	{
		Document.FinishInteraction();
	}

	void SelectObject(std::optional<FSceneHandle> InHandle) override
	{
		Document.ReplaceSelection(FSceneSelection(InHandle));
	}

	void SetSelection(FSceneSelection InSelection) override
	{
		Document.ReplaceSelection(std::move(InSelection));
	}

	void ClickOutlinerObject(FSceneHandle InHandle, bool bInToggle, bool) override
	{
		++Clicks;
		if (bInToggle)
		{
			auto Selected = Document.Selection();
			Selected.Toggle(InHandle);
			SetSelection(std::move(Selected));
		}
		else
		{
			SelectObject(InHandle);
		}
	}

	void BeginReparentDrag() override
	{
		++DragStarts;
	}

	void UpdateDocumentInteraction() override
	{
		Document.SetInteractionState(Controller.HasGesture(), false);
	}

	void Frame(std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({640, 480}, {640, 480}, 1.f / 60, InEvents);
		Controller.Update(Gui, Document, *this, InEvents, true, true);
		Gui.BeginPanel("Hierarchy", {0, 0}, {500, 450});
		if (Gui.BeginTable("Objects", "Item Label", "Type"))
		{
			bSceneOpen = FOutlinerSceneItem(Document).Draw(Gui, bExpandScene);
			bExpandScene = false;
			const auto Root = Controller.RouteRoot(Gui, Document);
			RootBounds = *Root.RootBounds;
			if (Root.Error)
			{
				Error = Root.Error;
			}
			Gui.NextColumn();
			Gui.Text("Scene");
			if (bSceneOpen)
			{
				DrawObject(Source, "Source", SourceBounds);
				DrawObject(Parent, "Parent", ParentBounds);
				Gui.EndTree();
			}
			Gui.EndTable();
		}
		Gui.EndPanel();
		if (bResetBeforeFinish && Gui.PointerState().bReleased)
		{
			Controller.Reset(&Gui);
		}
		if (const auto Result = Controller.Finish(Gui, Document, *this))
		{
			Error = Result;
		}
		Gui.Render();
	}

	void DrawObject(FSceneHandle InHandle, const char* InLabel, FVec4& OutBounds)
	{
		if (!Target.FindNode(InHandle))
		{
			return;
		}
		Gui.NextRow();
		Gui.NextColumn();
		Gui.Selectable(InLabel, Document.Selection().Contains(InHandle));
		OutBounds = Gui.LastItemBounds();
		const auto Feedback = Controller.RouteRow(Gui, Document, *this, InHandle, false, true);
		if (Feedback.Error)
		{
			Error = Feedback.Error;
		}
		Gui.NextColumn();
		Gui.Text("Group");
	}

	void Move(FVec2 InPoint)
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseMove;
		Event.X = InPoint.X;
		Event.Y = InPoint.Y;
		Frame(std::span(&Event, 1));
	}

	void Button(bool bInDown)
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseButton;
		Event.bDown = bInDown;
		Frame(std::span(&Event, 1));
	}

	void Start()
	{
		const auto Point = Center(SourceBounds);
		Move(Point);
		Button(true);
		HYP_CHECK(Controller.HasGesture());
		Move({Point.X + 20, Point.Y});
		HYP_CHECK(Controller.IsDragging() && Gui.DragPayload());
	}
};

bool SameBounds(FVec4 InA, FVec4 InB)
{
	return InA.X == InB.X && InA.Y == InB.Y && InA.Z == InB.Z && InA.W == InB.W;
}

void CheckStableSceneLayout()
{
	FReparentFixture Fixture;
	const auto RootBounds = Fixture.RootBounds;
	const auto SourceBounds = Fixture.SourceBounds;
	const auto ParentBounds = Fixture.ParentBounds;
	const auto Revision = Fixture.Target.Revision();
	Fixture.Start();
	HYP_CHECK(SameBounds(Fixture.RootBounds, RootBounds));
	HYP_CHECK(SameBounds(Fixture.SourceBounds, SourceBounds));
	HYP_CHECK(SameBounds(Fixture.ParentBounds, ParentBounds));
	Fixture.Move(Center(Fixture.RootBounds));
	Fixture.Frame();
	Fixture.Button(false);
	HYP_CHECK(SameBounds(Fixture.RootBounds, RootBounds));
	HYP_CHECK(SameBounds(Fixture.SourceBounds, SourceBounds));
	HYP_CHECK(SameBounds(Fixture.ParentBounds, ParentBounds));
	HYP_CHECK(Fixture.Target.Revision() == Revision && Fixture.Document.GetState().History.empty());
	HYP_CHECK(!Fixture.Document.IsDirty() && Fixture.Target.Nodes().size() == 2);
	HYP_CHECK(Fixture.Document.Selection().All().size() == 1);
	Fixture.Start();
	Fixture.Controller.Cancel(&Fixture.Gui);
	Fixture.Button(false);
	HYP_CHECK(SameBounds(Fixture.RootBounds, RootBounds));
	HYP_CHECK(SameBounds(Fixture.ParentBounds, ParentBounds));
}

void CheckScenePresentation()
{
	FReparentFixture Fixture;
	const auto SceneItem = FOutlinerSceneItem(Fixture.Document);
	HYP_CHECK(SceneItem.Label == "Untitled Scene" && Fixture.bSceneOpen);
	Fixture.Document.SetPath("/Game/Scenes/Courtyard.hasset");
	HYP_CHECK(FOutlinerSceneItem(Fixture.Document).Id == SceneItem.Id);
	HYP_CHECK(FOutlinerSceneItem(Fixture.Document).Label == "Courtyard");
	const auto Revision = Fixture.Target.Revision();
	const auto Selection = Fixture.Document.Selection();
	Fixture.Move(Center(Fixture.RootBounds));
	Fixture.Button(true);
	Fixture.Move({Center(Fixture.RootBounds).X + 20, Center(Fixture.RootBounds).Y});
	Fixture.Button(false);
	HYP_CHECK(!Fixture.Controller.HasGesture() && Fixture.DragStarts == 0 && Fixture.Clicks == 0);
	HYP_CHECK(Fixture.Document.Selection() == Selection);
	Fixture.Move({Fixture.RootBounds.X + 5, Center(Fixture.RootBounds).Y});
	Fixture.Button(true);
	Fixture.Button(false);
	HYP_CHECK(!Fixture.bSceneOpen && Fixture.Document.Selection() == Selection);
	Fixture.bExpandScene = true;
	Fixture.Frame();
	HYP_CHECK(Fixture.bSceneOpen && Fixture.Target.Revision() == Revision);
	HYP_CHECK(!Fixture.Document.IsDirty() && Fixture.Document.GetState().History.empty());
	const auto All = SelectAllSceneNodes(Fixture.Document, {Fixture.Document.Id(), Revision});
	HYP_CHECK(All.Count == 2 && Fixture.Document.Selection().All().size() == 2);
	Fixture.Move({Fixture.RootBounds.X + 5, Center(Fixture.RootBounds).Y});
	Fixture.Button(true);
	Fixture.Button(false);
	HYP_CHECK(!Fixture.bSceneOpen);
	Fixture.Document.Invalidate();
	HYP_CHECK(FOutlinerSceneItem(Fixture.Document).Id != SceneItem.Id);
	Fixture.Frame();
	HYP_CHECK(Fixture.bSceneOpen);
	Fixture.Document.ReplaceSelection({});
	Fixture.Target.Scene.Clear();
	Fixture.Frame();
	HYP_CHECK(Fixture.bSceneOpen && Fixture.RootBounds.Z > Fixture.RootBounds.X);
	HYP_CHECK(Fixture.Target.Nodes().empty());
}

void CheckNodeAndRootDelivery()
{
	FReparentFixture Fixture;
	Fixture.Start();
	Fixture.Move(Center(Fixture.ParentBounds));
	Fixture.Frame();
	Fixture.Button(false);
	HYP_CHECK(Fixture.Error && Fixture.Error->empty());
	HYP_CHECK(!Fixture.Controller.HasGesture() && !Fixture.Gui.DragPayload());
	HYP_CHECK(Fixture.Target.FindNode(Fixture.Source)->Parent() == "parent");
	HYP_CHECK(Fixture.Document.GetState().HistoryCursor == 1 && Fixture.Document.IsDirty());
	HYP_CHECK(Fixture.Controller.ConsumeExpansion("parent"));
	HYP_CHECK(!Fixture.Controller.ConsumeExpansion("parent"));
	Fixture.Document.Undo();
	HYP_CHECK(Fixture.Target.FindNode(Fixture.Source)->Parent().empty());
	Fixture.Document.Redo();
	HYP_CHECK(Fixture.Target.FindNode(Fixture.Source)->Parent() == "parent");
	Fixture.Start();
	Fixture.Move(Center(Fixture.RootBounds));
	Fixture.Frame();
	Fixture.Button(false);
	HYP_CHECK(Fixture.Target.FindNode(Fixture.Source)->Parent().empty());
	HYP_CHECK(Fixture.Document.GetState().HistoryCursor == 2);
}

void CheckResetBeforeDeliveryAndDetach()
{
	FReparentFixture Fixture;
	const auto Revision = Fixture.Target.Revision();
	Fixture.Start();
	Fixture.Move(Center(Fixture.ParentBounds));
	Fixture.bResetBeforeFinish = true;
	Fixture.Button(false);
	HYP_CHECK(!Fixture.Controller.HasGesture() && !Fixture.Gui.DragPayload());
	HYP_CHECK(Fixture.Target.Revision() == Revision && Fixture.Document.GetState().History.empty());
	HYP_CHECK(!Fixture.Document.IsDirty() && Fixture.Target.FindNode(Fixture.Source)->Parent().empty());
	Fixture.Controller.Reset(&Fixture.Gui);
	Fixture.Document.Detach(Fixture.Tasks);
	Fixture.Controller.Reset(nullptr);
	Fixture.Controller.Cancel(nullptr);
	HYP_CHECK(!Fixture.Controller.ConsumeExpansion("parent"));
}

void CheckInvalidation(unsigned InCase)
{
	FReparentFixture Fixture;
	Fixture.Start();
	const auto History = Fixture.Document.GetState().HistoryCursor;
	if (InCase == 0)
	{
		Fixture.Document.Invalidate();
	}
	else if (InCase == 1)
	{
		Fixture.Document.ReplaceSelection(FSceneSelection(Fixture.Parent));
	}
	else if (InCase == 2)
	{
		Fixture.Target.Scene.RemoveSubtree(Fixture.Parent);
	}
	FInputEvent Event;
	Event.Type = EEventType::Focus;
	Event.bDown = false;
	Fixture.Frame(InCase == 3 ? std::span(&Event, 1) : std::span<const FInputEvent>{});
	HYP_CHECK(!Fixture.Controller.HasGesture() && !Fixture.Gui.DragPayload());
	HYP_CHECK(Fixture.Document.GetState().HistoryCursor == History);
	HYP_CHECK(Fixture.Target.FindNode(Fixture.Source)->Parent().empty());
}

void CheckClickAndResetExpansion()
{
	FReparentFixture Fixture;
	Fixture.Move(Center(Fixture.SourceBounds));
	Fixture.Button(true);
	Fixture.Button(false);
	HYP_CHECK(Fixture.Clicks == 1 && Fixture.DragStarts == 0 && !Fixture.Controller.HasGesture());
	Fixture.Start();
	Fixture.Move(Center(Fixture.ParentBounds));
	Fixture.Frame();
	Fixture.Button(false);
	Fixture.Controller.Reset(&Fixture.Gui);
	HYP_CHECK(!Fixture.Controller.ConsumeExpansion("parent"));
}
} // namespace

int main()
{
	try
	{
		CheckAcceptanceBoundsIdentities();
		CheckStableSceneLayout();
		CheckScenePresentation();
		CheckNodeAndRootDelivery();
		CheckResetBeforeDeliveryAndDetach();
		for (unsigned Case = 0; Case < 4; ++Case)
		{
			CheckInvalidation(Case);
		}
		CheckClickAndResetExpansion();
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
