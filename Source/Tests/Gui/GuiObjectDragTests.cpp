#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

struct FObjectDragFixture
{
	FGui Gui;
	FVec4 Source;
	FVec4 Target;
	unsigned Deliveries{};
	bool bPreview{};
	bool bImageSource{};
	bool bExternal{};
	bool bExternalStarted{};
	std::string Value = "Cube";

	FObjectDragFixture()
	{
		Gui.FontImage();
		Frame();
		Frame();
	}

	void Frame(std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 600}, {1600, 1200}, 1.f / 60, InEvents);
		Gui.BeginPanel("Source", {0, 0}, {220, 400});
		if (bImageSource)
		{
			Gui.Image(3, {40, 40});
		}
		else
		{
			Gui.Selectable("Cube", false);
		}
		Source = Gui.LastItemBounds();
		if (!bExternal || bExternalStarted)
		{
			Gui.DragSource("Object", Value, "Cube", bExternal);
		}
		if (bExternal && Gui.PointerState().bReleased)
		{
			bExternalStarted = false;
		}
		Gui.Image(3, {24, 24});
		Gui.DragSource("Object", "Other", "Other");
		Gui.EndPanel();
		Gui.BeginPanel("Target", {250, 0}, {500, 500});
		Target = Gui.Image(2).Bounds;
		const auto Drop = Gui.DropTarget("Object");
		bPreview = Drop.has_value();
		if (Drop && Drop->bDelivery)
		{
			HYP_CHECK(Drop->Value == "Cube");
			++Deliveries;
		}
		Gui.EndPanel();
		Gui.Render();
	}

	void Move(FVec2 InPosition)
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseMove;
		Event.X = InPosition.X;
		Event.Y = InPosition.Y;
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
		Move({(Source.X + Source.Z) / 2, (Source.Y + Source.W) / 2});
		Button(true);
		bExternalStarted = bExternal;
		Move({(Source.X + Source.Z) / 2 + 12, (Source.Y + Source.W) / 2});
		HYP_CHECK(Gui.DragPayload() && Gui.DragPayload()->Value == "Cube");
		Value = "Changed source after start";
		Move({(Target.X + Target.Z) / 2, (Target.Y + Target.W) / 2});
		Frame();
		HYP_CHECK(bPreview && Gui.DragPayload()->Value == "Cube");
	}
};

void CheckHierarchyTargets()
{
	FGui Gui;
	Gui.FontImage();
	FVec4 ParentBounds;
	FVec4 FirstRow;
	bool bDragging{};
	bool bExpanded{};
	const auto Frame = [&](std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, InEvents);
		Gui.BeginPanel("Source", {0, 0}, {220, 400});
		Gui.Selectable("Source node", true);
		if (bDragging)
		{
			Gui.DragSource("Hierarchy", "1", "Selected nodes", true);
		}
		Gui.EndPanel();
		Gui.BeginPanel("Target", {250, 0}, {500, 500});
		if (Gui.BeginTable("Tree", "Name", "Type"))
		{
			Gui.NextRow();
			Gui.NextColumn();
			bool bClicked{};
			bExpanded = Gui.TreeItem("Parent", "Parent", false, false, bClicked, false);
			ParentBounds = Gui.LastItemBounds();
			Gui.DropTarget("Hierarchy");
			if (bExpanded)
			{
				Gui.EndTree();
			}
			for (unsigned Index = 0; Index < 80; ++Index)
			{
				Gui.NextRow();
				Gui.NextColumn();
				Gui.Selectable(("Node " + std::to_string(Index)).c_str(), false);
				if (!Index)
				{
					FirstRow = Gui.LastItemBounds();
				}
				Gui.DropTarget("Hierarchy");
			}
			Gui.ScrollDragTarget();
			Gui.EndTable();
		}
		Gui.EndPanel();
		Gui.Render();
	};
	Frame();
	Frame();
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = 100;
	Move.Y = 20;
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.bDown = true;
	const std::array Press{Move, Button};
	Frame(Press);
	bDragging = true;
	Move.X = (ParentBounds.X + ParentBounds.Z) / 2;
	Move.Y = (ParentBounds.Y + ParentBounds.W) / 2;
	Frame(std::span(&Move, 1));
	for (unsigned Index = 0; Index < 60; ++Index)
	{
		Frame();
	}
	HYP_CHECK(bExpanded);
	const auto Before = FirstRow.Y;
	Move.Y = 480;
	Frame(std::span(&Move, 1));
	for (unsigned Index = 0; Index < 20; ++Index)
	{
		Frame();
	}
	HYP_CHECK(FirstRow.Y < Before - 20);
	Gui.CancelDragDrop();
}
} // namespace

void CheckObjectDrag()
{
	CheckHierarchyTargets();
	{
		FObjectDragFixture External;
		External.bExternal = true;
		External.Start();
		External.Button(false);
		External.Frame();
		HYP_CHECK(External.Deliveries == 1 && !External.Gui.DragPayload());
	}
	FObjectDragFixture Test;
	Test.Start();
	Test.Button(false);
	Test.Frame();
	HYP_CHECK(Test.Deliveries == 1 && !Test.Gui.DragPayload());
	Test.Value = "Cube";
	Test.bImageSource = true;
	Test.Frame();
	Test.Frame();
	Test.Start();
	Test.Gui.CancelDragDrop();
	Test.Button(false);
	Test.Frame();
	HYP_CHECK(Test.Deliveries == 1 && !Test.Gui.DragPayload());
	Test.Value = "Cube";
	Test.Start();
	FInputEvent Escape;
	Escape.Type = EEventType::Key;
	Escape.Key = EKey::Escape;
	Escape.bDown = true;
	Test.Frame(std::span(&Escape, 1));
	Test.Button(false);
	HYP_CHECK(Test.Deliveries == 1 && !Test.Gui.DragPayload());
}
