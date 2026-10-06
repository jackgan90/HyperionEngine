#include "EditorAcceptanceHarness.h"

namespace Hyperion
{
namespace
{
void Check(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(std::string("Light marker acceptance: ") + InMessage);
	}
}

bool Contains(FVec4 InBounds, FVec2 InPoint)
{
	return InPoint.X >= InBounds.X && InPoint.X < InBounds.Z && InPoint.Y >= InBounds.Y && InPoint.Y < InBounds.W;
}

bool ContainsTriangle(FVec2 InA, FVec2 InB, FVec2 InC, FVec2 InPoint)
{
	const auto Side = [InPoint](FVec2 InStart, FVec2 InEnd)
	{
		return (InEnd.X - InStart.X) * (InPoint.Y - InStart.Y) - (InEnd.Y - InStart.Y) * (InPoint.X - InStart.X);
	};
	const float A = Side(InA, InB);
	const float B = Side(InB, InC);
	const float C = Side(InC, InA);
	return (A >= 0 && B >= 0 && C >= 0) || (A <= 0 && B <= 0 && C <= 0);
}
} // namespace

void FEditorAcceptanceHarness::ExercisePlacementMarkers(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.PlacementMarker.Is(EPlacementMarkerState::PrepareOverlap) && Scenario.PlacementMarkerCase == 0)
	{
		// Cancellation deliberately leaves the last light creation on the redo branch.
		Editor.Redo();
	}
	const auto Point = Editor.Scene->FindHandle(Scenario.PlacementExerciseIds.at(6));
	const auto Spot = Editor.Scene->FindHandle(Scenario.PlacementExerciseIds.at(7));
	Check(Editor.Scene->FindNode(Point) && Editor.Scene->FindNode(Spot), "overlap fixtures unavailable");
	if (Scenario.PlacementMarker.Is(EPlacementMarkerState::PrepareOverlap))
	{
		Editor.SelectObject(std::nullopt);
		Scenario.PlacementMarkerTransforms = {Editor.Scene->FindNode(Point)->Local(),
		                                      Editor.Scene->FindNode(Spot)->Local()};
		const auto Ray = MakeViewportRay(Editor.Viewport.ViewCamera, {.42f, .3f}, Editor.Viewport.ViewportSize.Width,
		                                 Editor.Viewport.ViewportSize.Height);
		Check(Ray.has_value(), "overlap ray unavailable");
		Editor.Scene->SetLocalTransform(
		    Point,
		    Translation(Add(Ray->Origin, ScaleVector(Ray->Direction, Scenario.PlacementMarkerCase == 1 ? 4.f : 2.f))));
		Editor.Scene->SetLocalTransform(
		    Spot,
		    Translation(Add(Ray->Origin, ScaleVector(Ray->Direction, Scenario.PlacementMarkerCase == 0 ? 4.f : 2.f))));
		Editor.Scene->Tick();
	}
	else if (Scenario.PlacementMarker.Is(EPlacementMarkerState::PointAtOverlap))
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseMove;
		Event.X = Editor.Viewport.ViewportRegion.Bounds.X +
		          (Editor.Viewport.ViewportRegion.Bounds.Z - Editor.Viewport.ViewportRegion.Bounds.X) * .42f;
		Event.Y = Editor.Viewport.ViewportRegion.Bounds.Y +
		          (Editor.Viewport.ViewportRegion.Bounds.W - Editor.Viewport.ViewportRegion.Bounds.Y) * .3f;
		InEvents.push_back(Event);
	}
	else if (Scenario.PlacementMarker.Is(EPlacementMarkerState::PressMarker) ||
	         Scenario.PlacementMarker.Is(EPlacementMarkerState::ReleaseMarker))
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseButton;
		Event.bDown = Scenario.PlacementMarker.Is(EPlacementMarkerState::PressMarker);
		InEvents.push_back(Event);
	}
	else
	{
		Check(Editor.Selection == (Scenario.PlacementMarkerCase == 1 ? Spot : Point),
		      "overlapping marker click selected a covered light");
		Editor.Scene->SetLocalTransform(Point, Scenario.PlacementMarkerTransforms[0]);
		Editor.Scene->SetLocalTransform(Spot, Scenario.PlacementMarkerTransforms[1]);
		Editor.Scene->Tick();
		Editor.SelectObject(std::nullopt);
		++Scenario.PlacementMarkerCase;
		Scenario.PlacementMarker.TransitionTo(EPlacementMarkerState::PrepareOverlap);
		return;
	}
	switch (Scenario.PlacementMarker.GetState())
	{
		case EPlacementMarkerState::PrepareOverlap:
			Scenario.PlacementMarker.TransitionTo(EPlacementMarkerState::PointAtOverlap);
			break;
		case EPlacementMarkerState::PointAtOverlap:
			Scenario.PlacementMarker.TransitionTo(EPlacementMarkerState::PressMarker);
			break;
		case EPlacementMarkerState::PressMarker:
			Scenario.PlacementMarker.TransitionTo(EPlacementMarkerState::ReleaseMarker);
			break;
		case EPlacementMarkerState::ReleaseMarker:
			Scenario.PlacementMarker.TransitionTo(EPlacementMarkerState::VerifyMarker);
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::CheckPlacementMarkerDraws(const FGuiDrawData& InData) const
{
	if (Scenario.PlacementMarkerCase >= 3 || !Scenario.PlacementMarker.Is(EPlacementMarkerState::PressMarker))
	{
		return;
	}
	const FVec2 Point{Editor.Viewport.ViewportRegion.Bounds.X +
	                      (Editor.Viewport.ViewportRegion.Bounds.Z - Editor.Viewport.ViewportRegion.Bounds.X) * .42f,
	                  Editor.Viewport.ViewportRegion.Bounds.Y +
	                      (Editor.Viewport.ViewportRegion.Bounds.W - Editor.Viewport.ViewportRegion.Bounds.Y) * .3f};
	const auto PointTexture = Editor.PlacementIcons.at("PointLight").Texture;
	const auto SpotTexture = Editor.PlacementIcons.at("SpotLight").Texture;
	std::uint64_t TopTexture{};
	for (const auto& Command : InData.Commands)
	{
		if ((Command.TextureId != PointTexture && Command.TextureId != SpotTexture) || !Contains(Command.Clip, Point))
		{
			continue;
		}
		for (std::size_t Index = Command.FirstIndex; Index + 2 < Command.FirstIndex + Command.IndexCount; Index += 3)
		{
			const auto A = InData.Vertices.at(InData.Indices.at(Index) + Command.VertexOffset).Position;
			const auto B = InData.Vertices.at(InData.Indices.at(Index + 1) + Command.VertexOffset).Position;
			const auto C = InData.Vertices.at(InData.Indices.at(Index + 2) + Command.VertexOffset).Position;
			if (ContainsTriangle(A, B, C, Point))
			{
				TopTexture = Command.TextureId;
			}
		}
	}
	Check(TopTexture == (Scenario.PlacementMarkerCase == 1 ? SpotTexture : PointTexture),
	      "the covered light was drawn on top");
}
} // namespace Hyperion
