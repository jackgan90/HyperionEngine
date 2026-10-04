#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

template<class T> void CheckBulkControl()
{
	struct FFixture
	{
		std::vector<T> Values{T{10}};
	};

	const auto Type = MakeRecord<FFixture>("test.bulk-gui", {Member("values", &FFixture::Values, Inspect("Values"))});
	FFixture Source;
	FRecordDraft Draft(Type, &Source);
	FGui Gui;
	Gui.FontImage();
	FVec4 Bounds;
	unsigned Changes{};
	const auto Frame = [&](std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, InEvents);
		Gui.BeginPanel("Bulk", {0, 0}, {600, 500});
		Gui.BeginLiveEdit();
		Changes += Gui.EditRecord(Draft, "fixture",
		                          [&](std::string_view, FVec4 InBounds)
		                          {
			                          Bounds = InBounds;
		                          });
		Gui.EndLiveEdit();
		Gui.EndPanel();
		Gui.Render();
	};
	Frame();
	Frame();
	const auto Closed = Bounds;
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = Closed.X + 10;
	Move.Y = (Closed.Y + Closed.W) / 2;
	Frame(std::span(&Move, 1));
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.bDown = true;
	Frame(std::span(&Button, 1));
	Button.bDown = false;
	Frame(std::span(&Button, 1));
	Frame();
	HYP_CHECK(Bounds.Y > Closed.Y && Changes == 0);
	// The observer now reports the numeric row inside the expanded sequence.
	Move.X = (Bounds.X + Bounds.Z) / 2;
	Move.Y = (Bounds.Y + Bounds.W) / 2;
	Frame(std::span(&Move, 1));
	Button.bDown = true;
	Frame(std::span(&Button, 1));
	Move.X += 30;
	Frame(std::span(&Move, 1));
	Button.bDown = false;
	Frame(std::span(&Button, 1));
	FFixture Candidate;
	Draft.ApplyToCandidate(&Candidate);
	HYP_CHECK(Changes > 0 && Candidate.Values.size() == 1 && Candidate.Values[0] > T{10});
	HYP_CHECK(std::get<FBulkData>(Draft.GetValues().at("values").Value).Element == BulkElement<T>());
	const auto PreviousChanges = Changes;
	Frame();
	HYP_CHECK(Changes == PreviousChanges);
}
} // namespace

void CheckBulkGuiControls()
{
	CheckBulkControl<std::uint8_t>();
	CheckBulkControl<std::int8_t>();
	CheckBulkControl<std::uint16_t>();
	CheckBulkControl<std::int16_t>();
	CheckBulkControl<std::uint32_t>();
	CheckBulkControl<std::int32_t>();
	CheckBulkControl<std::uint64_t>();
	CheckBulkControl<std::int64_t>();
	CheckBulkControl<float>();
	CheckBulkControl<double>();
}
