#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

struct FDragControls
{
	FGui Gui;
	FVec3 Vector{1, 2, 3};
	double Number = 10;
	std::int64_t Integer = 10;
	std::uint64_t Unsigned = 10;
	FMat4 Matrix;
	std::array<FVec4, 3> Bounds;
	FGuiEditState Edit;
	bool bDisabled{};

	FDragControls()
	{
		Gui.FontImage();
		Frame();
		Frame();
	}

	void Frame(std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 800}, {800, 800}, 1.f / 60, InEvents);
		Gui.BeginPanel("Drag controls", {0, 0}, {700, 750});
		Gui.BeginDisabled(bDisabled);
		Gui.BeginLiveEdit();
		Gui.InputVectorRow("Vector", Vector, {}, {}, Bounds);
		Gui.InputNumber("Number", Number);
		Gui.InputInteger("Integer", Integer);
		Gui.InputInteger("Unsigned", Unsigned);
		Gui.InputMatrix("Matrix", Matrix);
		Edit = Gui.EndLiveEdit();
		Gui.EndDisabled();
		Gui.EndPanel();
		Gui.Render();
	}
};

void CheckVectorDrag()
{
	FDragControls Test;
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = (Test.Bounds[0].X + Test.Bounds[0].Z) / 2;
	Move.Y = (Test.Bounds[0].Y + Test.Bounds[0].W) / 2;
	Test.Frame(std::span(&Move, 1));
	HYP_CHECK(Test.Gui.MouseCursor() == EMouseCursor::ResizeHorizontal);
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.bDown = true;
	Test.Frame(std::span(&Button, 1));
	HYP_CHECK(Test.Gui.MouseCursor() == EMouseCursor::Hidden);
	const auto Interaction = Test.Edit.ActiveInteraction;
	HYP_CHECK(Interaction != 0);
	Move.X += 30;
	Test.Frame(std::span(&Move, 1));
	HYP_CHECK(Test.Vector.X > 1 && Test.Vector.Y == 2 && Test.Vector.Z == 3);
	HYP_CHECK(Test.Edit.ChangedInteraction == Interaction);
	HYP_CHECK(Test.Gui.MouseCursor() == EMouseCursor::Hidden);
	const float Increased = Test.Vector.X;
	Move.X -= 20;
	Test.Frame(std::span(&Move, 1));
	HYP_CHECK(Test.Vector.X < Increased && Test.Edit.ChangedInteraction == Interaction);
	Button.bDown = false;
	Test.Frame(std::span(&Button, 1));
	HYP_CHECK(Test.Gui.MouseCursor() == EMouseCursor::ResizeHorizontal);
	HYP_CHECK(Test.Edit.ActiveInteraction == 0);
	Button.bDown = true;
	Test.Frame(std::span(&Button, 1));
	HYP_CHECK(Test.Edit.ActiveInteraction != Interaction);
	Test.Gui.FinishEditing();
	Test.Vector.X = 1;
	Button.bDown = false;
	Test.Frame(std::span(&Button, 1));
	HYP_CHECK(Test.Vector.X == 1 && Test.Edit.ChangedInteraction == 0);
	Test.bDisabled = true;
	Test.Frame();
	HYP_CHECK(Test.Gui.MouseCursor() == EMouseCursor::Arrow);
	Button.bDown = true;
	Test.Frame(std::span(&Button, 1));
	Move.X += 30;
	Test.Frame(std::span(&Move, 1));
	HYP_CHECK(Test.Vector.X == 1 && Test.Edit.ActiveInteraction == 0);
}
} // namespace

void CheckDragControls()
{
	CheckVectorDrag();
}
