#include "EditorReparentController.h"
#include "Support/TestSupport.h"
#include <iostream>

using namespace Hyperion;

namespace
{
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
		Gui.Selectable("Source", Document.Selection().Contains(Source));
		SourceBounds = Gui.LastItemBounds();
		Controller.RouteRow(Gui, Document, *this, Source, false, true);
		const auto Root = Controller.DrawRoot(Gui, Document);
		if (Root.RootBounds)
		{
			RootBounds = *Root.RootBounds;
		}
		if (Root.Error)
		{
			Error = Root.Error;
		}
		Gui.Selectable("Parent", Document.Selection().Contains(Parent));
		ParentBounds = Gui.LastItemBounds();
		const auto Feedback = Controller.RouteRow(Gui, Document, *this, Parent, false, true);
		if (Feedback.Error)
		{
			Error = Feedback.Error;
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
