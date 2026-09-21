#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Gui/PathDisplay.h"
#include "Support/TestSupport.h"
#include <array>

namespace
{
using namespace Hyperion;

struct FPathInput
{
	FGui Gui;
	std::string Value = "/Game/Scenes/Old.hasset";
	FVec4 Bounds;
	bool bMixed{};

	explicit FPathInput(bool bInMixed) : bMixed(bInMixed)
	{
		Gui.SetPathDisplayRoot("/Game");
		Gui.FontImage();
		Frame({});
		Frame({});
	}

	void Frame(std::span<const FInputEvent> InEvents)
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, InEvents);
		Gui.BeginPanel("Path input", {0, 0}, {750, 500});
		if (bMixed)
		{
			auto Node = WriteValue(Value);
			Gui.EditMixedScalar(Node, {.Kind = ERecordValueKind::String}, {.Widget = EPropertyWidget::Path}, true);
			Value = ReadValue<std::string>(Node);
		}
		else
		{
			Gui.InputText("Path", Value, true, true);
		}
		Bounds = Gui.LastItemBounds();
		Gui.EndPanel();
		Gui.Render();
	}

	void Type(const char* InText)
	{
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (Bounds.X + Bounds.Z) / 2;
		Move.Y = (Bounds.Y + Bounds.W) / 2;
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = true;
		Frame(std::array{Move, Button});
		Button.bDown = false;
		Frame(std::span(&Button, 1));
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Modifiers = 1;
		Key.Key = EKey::A;
		Key.bDown = true;
		Frame(std::span(&Key, 1));
		Key.Modifiers = 0;
		Key.bDown = false;
		Frame(std::span(&Key, 1));
		Key.Key = EKey::Backspace;
		Key.bDown = true;
		Frame(std::span(&Key, 1));
		Key.bDown = false;
		FInputEvent Text;
		Text.Type = EEventType::Text;
		Text.Text = InText;
		Frame(std::array{Key, Text});
		Key.Key = EKey::Enter;
		Key.bDown = true;
		Frame(std::span(&Key, 1));
		Key.bDown = false;
		Frame(std::span(&Key, 1));
	}
};

FGuiDrawData DrawPaths(bool bInRelative)
{
	FGui Gui;
	Gui.FontImage();
	Gui.SetPathDisplayRoot(bInRelative ? "/Game" : "");
	const std::string Prefix = bInRelative ? "/Game/" : "";
	FGuiDrawData Result;
	for (unsigned Frame = 0; Frame < 2; ++Frame)
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, {});
		Gui.BeginPanel("Path presentation", {0, 0}, {750, 500});
		Gui.Text("Saved: " + Prefix + "Scenes/Scene.hasset");
		Gui.TextWrapped("Could not read '" + Prefix + "Missing.hasset'");
		auto Value = Prefix + "Scenes/Scene.hasset";
		Gui.InputText("Path", Value, true, true);
		HYP_CHECK(Value == Prefix + "Scenes/Scene.hasset");
		Gui.BeginDisabled(true);
		Gui.InputText("Diagnostic", Value);
		Gui.EndDisabled();
		Gui.EndPanel();
		Gui.StatusBar("Ready | " + Prefix + "Scenes/Scene.hasset");
		Result = Gui.Render();
	}
	return Result;
}
} // namespace

void CheckPathDisplay()
{
	const FGuiPathDisplay Display{"/Game"};
	HYP_CHECK(Display.Text("Saved: /Game/Scenes/Scene.hasset\n'/Game/Other.hasset'") ==
	          "Saved: Scenes/Scene.hasset\n'Other.hasset'");
	HYP_CHECK(Display.Text("F:/Game/Local.hasset /Engine/Fonts/Roboto.ttf /Gameplay/Other.hasset") ==
	          "F:/Game/Local.hasset /Engine/Fonts/Roboto.ttf /Gameplay/Other.hasset");
	HYP_CHECK(Display.Resolve("").empty());
	HYP_CHECK(Display.Resolve("Scenes/New.hasset") == "/Game/Scenes/New.hasset");
	HYP_CHECK(Display.Resolve("/Engine/Test.hasset") == "/Engine/Test.hasset");
	HYP_CHECK(Display.Resolve("F:/Test.hasset") == "F:/Test.hasset");
	const auto Relative = DrawPaths(true);
	const auto Expected = DrawPaths(false);
	HYP_CHECK(Relative.Indices == Expected.Indices && Relative.Vertices.size() == Expected.Vertices.size());
	for (std::size_t Index = 0; Index < Relative.Vertices.size(); ++Index)
	{
		const auto& A = Relative.Vertices[Index];
		const auto& B = Expected.Vertices[Index];
		HYP_CHECK(A.Position.X == B.Position.X && A.Position.Y == B.Position.Y && A.Uv.X == B.Uv.X &&
		          A.Uv.Y == B.Uv.Y && A.Color == B.Color);
	}
	for (const bool bMixed : {false, true})
	{
		FPathInput Input(bMixed);
		Input.Type("Scenes/New.hasset");
		HYP_CHECK(Input.Value == "/Game/Scenes/New.hasset");
		Input.Type("");
		HYP_CHECK(Input.Value.empty());
		Input.Type("Scenes/AfterClear.hasset");
		HYP_CHECK(Input.Value == "/Game/Scenes/AfterClear.hasset");
	}
}
