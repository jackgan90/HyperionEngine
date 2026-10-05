#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"
#include <algorithm>

namespace
{
using namespace Hyperion;

struct FChoiceFixture
{
	std::int64_t Value = 42;
};

void CheckChoice(bool bInMixed, bool bInReversed, bool bInReadOnly, bool bInUnknown = false)
{
	std::vector<FPropertyChoice> Choices{{WriteValue(std::int64_t{-7}), "Negative"},
	                                     {WriteValue(std::int64_t{42}), "Answer"},
	                                     {WriteValue(std::int64_t{300}), "Large"}};
	if (bInReversed)
	{
		std::reverse(Choices.begin(), Choices.end());
	}
	if (bInUnknown)
	{
		for (auto& Choice : Choices)
		{
			Choice.Label.append(256, 'x');
		}
	}
	auto Options = Inspect("Choice");
	Options.Inspector->Choices = Choices;
	Options.Inspector->bReadOnly = bInReadOnly;
	const auto Type = MakeRecord<FChoiceFixture>("test.gui-choice", {Member("value", &FChoiceFixture::Value, Options)});
	FChoiceFixture Source;
	Source.Value = bInUnknown ? 12345 : 42;
	FRecordDraft Draft(Type, &Source);
	FGui Gui;
	Gui.FontImage();
	FVec4 Bounds;
	unsigned Changes{};
	const auto Frame = [&](std::span<const FInputEvent> InEvents)
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, InEvents);
		Gui.BeginPanel("Choices", {0, 0}, {600, 500});
		Gui.BeginLiveEdit();
		if (bInMixed)
		{
			Gui.BeginDisabled(bInReadOnly);
			Gui.BeginPropertyRow("Choice");
			Changes += Gui.EditMixedScalar(Draft.GetValues().at("value"), {.Kind = ERecordValueKind::Integer},
			                               *Options.Inspector, true);
			Bounds = Gui.LastItemBounds();
			Gui.EndPropertyRow();
			Gui.EndDisabled();
		}
		else
		{
			Changes += Gui.EditRecord(Draft, "fixture",
			                          [&](std::string_view, FVec4 InBounds)
			                          {
				                          Bounds = InBounds;
			                          });
		}
		Gui.EndLiveEdit();
		Gui.EndPanel();
		Gui.Render();
	};
	const auto Click = [&](float InX, float InY)
	{
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = InX;
		Move.Y = InY;
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = true;
		Frame(std::array{Move, Button});
		Button.bDown = false;
		Frame(std::span(&Button, 1));
	};
	Frame({});
	Frame({});
	const auto Closed = Bounds;
	Click((Closed.X + Closed.Z) / 2, (Closed.Y + Closed.W) / 2);
	Frame({});
	HYP_CHECK(Changes == 0 && ReadValue<std::int64_t>(Draft.GetValues().at("value")) == Source.Value);
	// The default GUI style has 16 pixels of vertical popup padding before the first row.
	Click((Closed.X + Closed.Z) / 2, Closed.W + 24);
	Frame({});
	auto Candidate = Source;
	Draft.ApplyToCandidate(&Candidate);
	HYP_CHECK(Changes == (bInReadOnly ? 0u : 1u));
	HYP_CHECK(Candidate.Value == (bInReadOnly ? Source.Value : ReadValue<std::int64_t>(Choices.front().Value)));
}
} // namespace

void CheckChoiceControls()
{
	for (const bool bMixed : {false, true})
	{
		for (const bool bReversed : {false, true})
		{
			CheckChoice(bMixed, bReversed, false);
			CheckChoice(bMixed, bReversed, true);
			CheckChoice(bMixed, bReversed, false, true);
			CheckChoice(bMixed, bReversed, true, true);
		}
	}
}
