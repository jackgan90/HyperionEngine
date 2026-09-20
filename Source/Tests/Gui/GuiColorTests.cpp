#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <map>

namespace
{
using namespace Hyperion;

class FColorControls
{
public:
	FGui Gui;
	FVec3 Color{.25f, .5f, .75f};
	std::map<std::string, FVec4> Bounds;
	bool bChanged{};
	bool bDisabled{};
	bool bLive{};
	FGuiEditState Edit;
	std::array<bool, 3> Mixed{};
	std::array<bool, 3> Edited{};

	FColorControls()
	{
		Gui.FontImage();
		Frame();
		Frame();
	}

	void Frame(std::span<const FInputEvent> InEvents = {})
	{
		Bounds.clear();
		Gui.BeginFrame({800, 800}, {800, 800}, 1.f / 60, InEvents);
		Gui.BeginPanel("Color controls", {0, 0}, {700, 750});
		Gui.BeginDisabled(bDisabled);
		if (bLive)
		{
			Gui.BeginLiveEdit();
		}
		bChanged |= Gui.InputColor(
		    "Color##test", Color,
		    [&](std::string_view InId, FVec4 InBounds)
		    {
			    Bounds[std::string(InId)] = InBounds;
		    },
		    Mixed, &Edited);
		for (unsigned Channel = 0; Channel < 3; ++Channel)
		{
			if (Edited[Channel])
			{
				Mixed[Channel] = false;
			}
		}
		if (bLive)
		{
			Edit = Gui.EndLiveEdit();
		}
		Gui.EndDisabled();
		Gui.EndPanel();
		Gui.Render();
	}

	void Click(const char* InId, float InX = .5f, float InY = .5f)
	{
		const auto Box = Bounds.at(InId);
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = Box.X + (Box.Z - Box.X) * InX;
		Move.Y = Box.Y + (Box.W - Box.Y) * InY;
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = true;
		const std::array Down{Move, Button};
		Frame(Down);
		Button.bDown = false;
		Frame(std::span(&Button, 1));
		Frame();
	}

	void Type(const char* InId, const char* InText, bool bInCommit = true)
	{
		Click(InId);
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = EKey::A;
		Key.Modifiers = 1;
		Key.bDown = true;
		Frame(std::span(&Key, 1));
		Key.Modifiers = 0;
		Key.bDown = false;
		FInputEvent Text;
		Text.Type = EEventType::Text;
		Text.Text = InText;
		const std::array Replace{Key, Text};
		Frame(Replace);
		if (bInCommit)
		{
			Key.Key = EKey::Enter;
			Key.bDown = true;
			Frame(std::span(&Key, 1));
			Key.bDown = false;
			Frame(std::span(&Key, 1));
		}
		Frame();
	}
};

void CheckColorChannels()
{
	FColorControls Test;
	HYP_CHECK(!Test.bChanged && Test.Color.X == .25f && !Test.Bounds.contains("R"));
	Test.Click("expand", .05f);
	HYP_CHECK(Test.Bounds.contains("R") && Test.Bounds.contains("G") && Test.Bounds.contains("B"));
	Test.Type("R", "128");
	HYP_CHECK(Test.bChanged && std::abs(Test.Color.X - .2158605f) < 1e-6f);
	HYP_CHECK(Test.Color.Y == .5f && Test.Color.Z == .75f);
	Test.Type("G", "999");
	Test.Type("B", "-20");
	HYP_CHECK(Test.Color.Y == 1 && Test.Color.Z == 0);
	Test.bChanged = false;
	Test.bDisabled = true;
	Test.Type("R", "255");
	HYP_CHECK(!Test.bChanged && std::abs(Test.Color.X - .2158605f) < 1e-6f);
}

void CheckColorPopup()
{
	FColorControls Test;
	Test.Click("swatch");
	HYP_CHECK(Test.Bounds.contains("picker"));
	Test.Click("picker", .08f, .3f);
	HYP_CHECK(!Test.bChanged && Test.Color.X == .25f);
	Test.Click("cancel");
	HYP_CHECK(!Test.bChanged && Test.Color.X == .25f && Test.Color.Y == .5f && Test.Color.Z == .75f);
	Test.Click("swatch");
	Test.Click("picker", .08f, .3f);
	Test.Click("accept");
	HYP_CHECK(Test.bChanged);
	HYP_CHECK(Test.Color.X != .25f || Test.Color.Y != .5f || Test.Color.Z != .75f);
	Test.Color = {2.f, .1234567f, .8f};
	Test.bChanged = false;
	Test.Frame();
	Test.Click("swatch");
	Test.Click("accept");
	HYP_CHECK(!Test.bChanged && Test.Color.X == 2 && Test.Color.Y == .1234567f && Test.Color.Z == .8f);
}

void CheckLiveInputRestoration()
{
	FColorControls Test;
	Test.bLive = true;
	Test.Frame();
	Test.Click("expand", .05f);
	Test.Type("R", "128", false);
	HYP_CHECK(Test.bChanged && std::abs(Test.Color.X - .2158605f) < 1e-6f);
	HYP_CHECK(Test.Edit.ActiveInteraction != 0);
	Test.Gui.FinishEditing();
	Test.Color = {.25f, .5f, .75f};
	Test.bChanged = false;
	Test.Frame();
	Test.Frame();
	HYP_CHECK(!Test.bChanged && Test.Color.X == .25f);
}

void CheckLiveColorPopup()
{
	FColorControls Test;
	Test.bLive = true;
	Test.Frame();
	Test.Click("swatch");
	const auto Interaction = Test.Edit.ActiveInteraction;
	HYP_CHECK(Interaction != 0);
	Test.Click("picker", .08f, .3f);
	HYP_CHECK(Test.bChanged && Test.Edit.ActiveInteraction == Interaction);
	HYP_CHECK(Test.Color.X != .25f || Test.Color.Y != .5f || Test.Color.Z != .75f);
	Test.Click("picker", .85f, .3f);
	HYP_CHECK(Test.Edit.ActiveInteraction == Interaction);
	Test.Click("accept");
	HYP_CHECK(Test.Edit.ActiveInteraction == 0);
	Test.Click("swatch");
	HYP_CHECK(Test.Edit.ActiveInteraction != Interaction);
	Test.Gui.FinishEditing();
	Test.Color = {.25f, .5f, .75f};
	Test.bChanged = false;
	Test.Frame();
	HYP_CHECK(!Test.bChanged && !Test.Bounds.contains("picker"));
}

void CheckMixedColor()
{
	FColorControls Test;
	Test.bLive = true;
	Test.Mixed = {true, true, false};
	Test.Frame();
	Test.Click("expand", .05f);
	Test.Type("R", "128");
	HYP_CHECK(Test.bChanged && !Test.Mixed[0] && Test.Mixed[1]);
	HYP_CHECK(Test.Color.Y == .5f && Test.Color.Z == .75f);
	Test.bChanged = false;
	Test.Click("swatch");
	Test.Click("accept");
	HYP_CHECK(Test.bChanged && !Test.Mixed[0] && !Test.Mixed[1] && !Test.Mixed[2]);
	HYP_CHECK(Test.Color.Y == .5f && Test.Color.Z == .75f);
}
} // namespace

void CheckColorControls()
{
	CheckColorChannels();
	CheckColorPopup();
	CheckLiveColorPopup();
	CheckLiveInputRestoration();
	CheckMixedColor();
}
