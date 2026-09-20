#include "Hyperion/Renderer/ViewportPlacement.h"
#include "Hyperion/Scene/ObjectPlacement.h"
#include "Hyperion/Scene/PrimitiveShapes.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <iostream>

using namespace Hyperion;

namespace
{
void CheckRegistry()
{
	FObjectPlacementRegistry Registry;
	Registry.AddCategory("Basic");
	Registry.AddCategory("Shapes");
	Registry.Add({"cube",
	              "Cube",
	              {"Basic", "Shapes"},
	              {},
	              {},
	              []
	              {
		              return FSceneNode{};
	              }});
	HYP_CHECK(Registry.Search("All", {}).size() == 1);
	HYP_CHECK(Registry.Search("Basic", "CuB").size() == 1);
	HYP_CHECK(Registry.Search("Shapes", {}).size() == 1);
	HYP_CHECK(Registry.Search("Lights", {}).empty());
	HYP_CHECK(!Registry.Find("absent"));
	bool bRejected{};
	try
	{
		Registry.Add(*Registry.Find("cube"));
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected && Registry.Search("All", {}).size() == 1);
	Registry.Remove("cube");
	HYP_CHECK(!Registry.Find("cube") && Registry.Search("All", {}).empty());
}

void CheckShapes()
{
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		const auto Mesh = MakePrimitiveShape(static_cast<EPrimitiveShape>(Index));
		HYP_CHECK(!Mesh.Indices.empty() && Mesh.Indices.size() % 3 == 0);
		HYP_CHECK(Mesh.Normals.size() == Mesh.Positions.size());
		for (const auto Value : Mesh.Tangents)
		{
			HYP_CHECK(std::isfinite(Value));
		}
		const auto Vertex = [&](std::uint32_t InIndex)
		{
			return FVec3{Mesh.Positions.at(InIndex * 3), Mesh.Positions.at(InIndex * 3 + 1),
			             Mesh.Positions.at(InIndex * 3 + 2)};
		};
		for (std::size_t Triangle = 0; Triangle < Mesh.Indices.size(); Triangle += 3)
		{
			const auto A = Vertex(Mesh.Indices[Triangle]);
			const auto B = Vertex(Mesh.Indices[Triangle + 1]);
			const auto C = Vertex(Mesh.Indices[Triangle + 2]);
			const auto Normal = Cross(Subtract(B, A), Subtract(C, A));
			HYP_CHECK(Length(Normal) > .00001f);
			HYP_CHECK(Index == 4 ? Normal.Y > 0 : Dot(Normal, Add(Add(A, B), C)) > 0);
		}
	}
}

void CheckPlacement()
{
	FSceneCameraView Camera;
	Camera.World = SceneCameraTransform({0, 3, 8}, {});
	FSceneRayResult Miss;
	Miss.Status = ESceneRayStatus::Miss;
	const FRay Ray{{0, 3, 0}, {0, -1, 0}, .1f, 100};
	const FBounds Bounds{{-.5f, -.5f, -.5f}, {.5f, .5f, .5f}, true};
	const auto Ground = ResolveViewportPlacement(Ray, Miss, Camera, Bounds);
	HYP_CHECK(Ground && !Ground->bSurface && std::abs(Ground->Position.Y - .502f) < .00001f);
	FSceneRayResult Hit;
	Hit.Status = ESceneRayStatus::Hit;
	Hit.Position = {0, 2, 0};
	Hit.Normal = {0, -1, 0};
	const auto Surface = ResolveViewportPlacement(Ray, Hit, Camera, Bounds);
	HYP_CHECK(Surface && Surface->bSurface && std::abs(Surface->Position.Y - 2.502f) < .00001f);
	Hit.bIncomplete = true;
	HYP_CHECK(!ResolveViewportPlacement(Ray, Hit, Camera, Bounds));
	HYP_CHECK(!ResolveViewportPlacement(Ray, {}, Camera, Bounds));
	Camera.World = SceneCameraTransform({0, 2, 8}, {0, 2, 0});
	const auto Fallback = ResolveViewportPlacement({{0, 2, 8}, {0, 0, -1}, .1f, 100}, Miss, Camera, Bounds);
	HYP_CHECK(Fallback && Fallback->Position.Y == 2);
	HYP_CHECK(!ProjectViewportPoint(Camera, {0, 0, 800, 600}, {0, 2, 9}));
	HYP_CHECK(!ProjectViewportPoint(Camera, {0, 0, 800, 600}, {1000, 2, 0}));
	const auto Center = ProjectViewportPoint(Camera, {0, 0, 800, 600}, {0, 2, 0});
	HYP_CHECK(Center && Center->X == 400 && Center->Y == 300);
	FViewportPlacementContext Context;
	Context.Document = 10;
	Context.Revision = 20;
	Context.Bounds = {0, 0, 800, 600};
	Context.Camera = Camera;
	FViewportPlacementSession Session;
	Session.Begin("Cube", Context);
	Session.SetPreview(Ground);
	HYP_CHECK(Session.IsCurrent("Cube", Context) && Session.GetPreview());
	Context.ApplicationScale = 1.5f;
	HYP_CHECK(!Session.IsCurrent("Cube", Context));
	Session.SetPreview({});
	HYP_CHECK(Session.IsActive() && !Session.GetPreview());
	Session.Cancel();
	HYP_CHECK(!Session.IsActive() && !Session.GetPreview());
}
} // namespace

int main()
{
	try
	{
		CheckRegistry();
		CheckShapes();
		CheckPlacement();
		std::cout << "PASS: placement registry, primitive geometry and viewport placement\n";
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
