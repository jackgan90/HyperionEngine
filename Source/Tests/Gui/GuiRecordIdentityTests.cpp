#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Scene/Model.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

const FRecordValueShape& RenamedMatrixShape()
{
	static const auto Shape = []
	{
		auto Result = RecordValueShape<FMat4>();
		Result.Record = []() -> const FRecordDescriptor&
		{
			static const auto Type = []
			{
				auto Renamed = RecordType<FMat4>();
				Renamed.Id = "test.renamed-matrix";
				return Renamed;
			}();
			return Type;
		};
		return Result;
	}();
	return Shape;
}

void CheckMatrixInspection(bool bInRename)
{
	struct FFixture
	{
		FMat4 Matrix = Identity();
	};

	// Override only the inspection shape; keep ordinary archive read/write callbacks.
	auto MatrixMember = Member("matrix", &FFixture::Matrix, Inspect("Matrix"));
	if (bInRename)
	{
		MatrixMember.Shape = &RenamedMatrixShape;
	}
	const auto Type = MakeRecord<FFixture>("test.matrix-gui", {MatrixMember});
	FFixture Source;
	FRecordDraft Draft(Type, &Source);
	FGui Gui;
	Gui.FontImage();
	FVec4 Bounds;
	unsigned Changes{};
	const auto Frame = [&](std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, InEvents);
		Gui.BeginPanel("Matrix inspection", {0, 0}, {600, 500});
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
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	// Bounds cover four numeric cells; the group midpoint is the gap between cells.
	Move.X = Bounds.X + (Bounds.Z - Bounds.X) / 8;
	Move.Y = (Bounds.Y + Bounds.W) / 2;
	Frame(std::span(&Move, 1));
	HYP_CHECK(Gui.MouseCursor() == EMouseCursor::ResizeHorizontal);
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.bDown = true;
	Frame(std::span(&Button, 1));
	Move.X += 30;
	Frame(std::span(&Move, 1));
	Button.bDown = false;
	Frame(std::span(&Button, 1));
	FFixture Candidate;
	Draft.ApplyToCandidate(&Candidate);
	HYP_CHECK(Changes > 0 && Candidate.Matrix.Values[12] > Source.Matrix.Values[12]);
	for (std::size_t Index = 0; Index < Source.Matrix.Values.size(); ++Index)
	{
		if (Index != 12)
		{
			HYP_CHECK(Candidate.Matrix.Values[Index] == Source.Matrix.Values[Index]);
		}
	}
	const auto PreviousChanges = Changes;
	Frame();
	HYP_CHECK(Changes == PreviousChanges);
}
} // namespace

void CheckRecordIdentityControls()
{
	CheckMatrixInspection(false);
	CheckMatrixInspection(true);
}
