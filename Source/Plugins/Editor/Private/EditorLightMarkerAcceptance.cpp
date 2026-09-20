#include "EditorApplication.h"

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

void FEditorPlugin::ExercisePlacementMarkers(std::vector<FInputEvent>& InEvents)
{
	if (PlacementMarkerStep == 0 && PlacementMarkerCase == 0)
	{
		// Cancellation deliberately leaves the last light creation on the redo branch.
		Redo();
	}
	const auto Point = Scene->FindHandle(PlacementExerciseIds.at(6));
	const auto Spot = Scene->FindHandle(PlacementExerciseIds.at(7));
	Check(Scene->FindNode(Point) && Scene->FindNode(Spot), "overlap fixtures unavailable");
	if (PlacementMarkerStep == 0)
	{
		SelectObject(std::nullopt);
		PlacementMarkerTransforms = {Scene->FindNode(Point)->Local(), Scene->FindNode(Spot)->Local()};
		const auto Ray = MakeViewportRay(ViewCamera, {.42f, .3f}, ViewportSize.Width, ViewportSize.Height);
		Check(Ray.has_value(), "overlap ray unavailable");
		Scene->SetLocalTransform(
		    Point, Translation(Add(Ray->Origin, ScaleVector(Ray->Direction, PlacementMarkerCase == 1 ? 4.f : 2.f))));
		Scene->SetLocalTransform(
		    Spot, Translation(Add(Ray->Origin, ScaleVector(Ray->Direction, PlacementMarkerCase == 0 ? 4.f : 2.f))));
		Scene->Tick();
	}
	else if (PlacementMarkerStep == 1)
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseMove;
		Event.X = ViewportRegion.Bounds.X + (ViewportRegion.Bounds.Z - ViewportRegion.Bounds.X) * .42f;
		Event.Y = ViewportRegion.Bounds.Y + (ViewportRegion.Bounds.W - ViewportRegion.Bounds.Y) * .3f;
		InEvents.push_back(Event);
	}
	else if (PlacementMarkerStep == 2 || PlacementMarkerStep == 3)
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseButton;
		Event.bDown = PlacementMarkerStep == 2;
		InEvents.push_back(Event);
	}
	else
	{
		Check(Selection == (PlacementMarkerCase == 1 ? Spot : Point),
		      "overlapping marker click selected a covered light");
		Scene->SetLocalTransform(Point, PlacementMarkerTransforms[0]);
		Scene->SetLocalTransform(Spot, PlacementMarkerTransforms[1]);
		Scene->Tick();
		SelectObject(std::nullopt);
		++PlacementMarkerCase;
		PlacementMarkerStep = 0;
		return;
	}
	++PlacementMarkerStep;
}

void FEditorPlugin::CheckPlacementMarkerDraws(const FGuiDrawData& InData) const
{
	if (PlacementMarkerCase >= 3 || PlacementMarkerStep != 2)
	{
		return;
	}
	const FVec2 Point{ViewportRegion.Bounds.X + (ViewportRegion.Bounds.Z - ViewportRegion.Bounds.X) * .42f,
	                  ViewportRegion.Bounds.Y + (ViewportRegion.Bounds.W - ViewportRegion.Bounds.Y) * .3f};
	const auto PointTexture = PlacementIcons.at("PointLight").Texture;
	const auto SpotTexture = PlacementIcons.at("SpotLight").Texture;
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
	Check(TopTexture == (PlacementMarkerCase == 1 ? SpotTexture : PointTexture), "the covered light was drawn on top");
}
} // namespace Hyperion
