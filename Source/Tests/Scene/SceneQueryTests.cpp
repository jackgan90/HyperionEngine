#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Renderer/ViewportRay.h"
#include "Hyperion/Scene/Scene.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <iostream>
#include <random>
#include <thread>

using namespace Hyperion;

namespace
{
std::shared_ptr<FModelAsset> Mesh(unsigned InTriangles = 1)
{
	auto Asset = std::make_shared<FModelAsset>();
	Asset->MaterialSlots = {{"", "Test.hasset", "hyperion.materialasset", ""}};
	FModelPrimitive Primitive;
	Primitive.Id = "triangle";
	Primitive.Material = 0;
	for (unsigned Index = 0; Index < InTriangles; ++Index)
	{
		const float X = float(Index) * 4;
		Primitive.Positions.insert(Primitive.Positions.end(), {X - 1, -1, 0, X + 1, -1, 0, X, 1, 0});
		Primitive.Indices.insert(Primitive.Indices.end(), {Index * 3, Index * 3 + 1, Index * 3 + 2});
	}
	Asset->Primitives.push_back(std::move(Primitive));
	FModelNode Node;
	Node.Id = "mesh";
	Node.Primitives = {0};
	Asset->Nodes.push_back(Node);
	Asset->Roots = {0};
	return Asset;
}

std::shared_ptr<FSceneModelData> Prepared(const std::shared_ptr<FModelAsset>& InAsset)
{
	auto Data = std::make_shared<FSceneModelData>(*PrepareSceneModel(InAsset));
	Data->QueryGeometry = PrepareSceneModelGeometry(*InAsset);
	return Data;
}

FSceneModel Model(const std::shared_ptr<const FSceneModelData>& InData, FVec3 InTranslation = {})
{
	FSceneModel Result;
	Result.Data = InData;
	Result.World = Translation(InTranslation);
	return Result;
}

void CheckQueries()
{
	FScene Scene;
	const auto Data = Prepared(Mesh());
	const auto Far = Scene.Add(Model(Data));
	const auto Near = Scene.Add(Model(Data, {0, 0, 2}));
	const FRay Ray{{0, 0, 5}, {0, 0, -1}, 0, 20};
	auto Hit = Scene.Raycast(Ray);
	HYP_CHECK(Hit.Status == ESceneRayStatus::Hit && Hit.Handle == Near && Hit.Distance == 3);
	HYP_CHECK(Hit.Stats.Bounds.IndexRebuilds == 1);
	Hit = Scene.Raycast(Ray);
	HYP_CHECK(Hit.Stats.Bounds.IndexRebuilds == 0 && Hit.Stats.Bounds.IndexRefits == 0);
	Scene.SetWorldTransform(Near, Translation({1, 0, 2}));
	Hit = Scene.Raycast(Ray);
	HYP_CHECK(Hit.Handle == Far && Hit.Distance == 5);
	Scene.SetWorldTransform(Near, Translation({10, 0, 2}));
	Scene.Acknowledge(Scene.GetRevision());
	Hit = Scene.Raycast(Ray);
	HYP_CHECK(Hit.Handle == Far && Hit.Distance == 5 && Hit.Stats.Bounds.IndexRefits == 1);
	Scene.SetName(Far, "renamed");
	Hit = Scene.Raycast(Ray);
	HYP_CHECK(Hit.Stats.Bounds.IndexRefits == 0 && Hit.Stats.Bounds.IndexRebuilds == 0);
	Scene.SetModelVisible(Far, false);
	HYP_CHECK(Scene.Raycast(Ray).Status == ESceneRayStatus::Miss);
	Scene.SetModelVisible(Far, true);
	auto Changed = *Scene.Find(Far);
	Changed.Sections = {{"triangle", false}};
	Scene.Update(Far, Changed);
	HYP_CHECK(Scene.Raycast(Ray).Status == ESceneRayStatus::Miss);
	Changed.Sections.clear();
	Changed.SourceNode = "mesh";
	Changed.SourcePrimitive = "triangle";
	Scene.Update(Far, Changed);
	HYP_CHECK(Scene.Raycast(Ray).Handle == Far);
	FSceneNode Parent;
	Parent.Id = "parent";
	const auto Group = Scene.AddNode(Parent);
	Scene.Reparent(Far, Group, ESceneReparentMode::KeepWorld);
	Scene.SetEnabled(Group, false);
	HYP_CHECK(Scene.Raycast(Ray).Status == ESceneRayStatus::Miss);
	Scene.SetEnabled(Group, true);
	Scene.SetLocalTransform(Group, Translation({0, 0, 1}));
	HYP_CHECK(Scene.Raycast(Ray).Distance == 4);
	Scene.Remove(Near);
	const auto Reused = Scene.Add(Model(Data, {0, 0, 3}));
	HYP_CHECK(Reused.Slot == Near.Slot && Reused.Generation != Near.Generation && !Scene.Find(Near));
	HYP_CHECK(Scene.Raycast(Ray).Handle == Reused);
	bool bWrongOwnerRejected{};
	std::thread Other(
	    [&]
	    {
		    try
		    {
			    Scene.Raycast(Ray);
		    }
		    catch (const std::logic_error&)
		    {
			    bWrongOwnerRejected = true;
		    }
	    });
	Other.join();
	HYP_CHECK(bWrongOwnerRejected);
	Scene.Clear();
	HYP_CHECK(Scene.Raycast(Ray).Status == ESceneRayStatus::Miss);
}

void CheckTransformsAndReadiness()
{
	FScene Scene;
	const auto Asset = Mesh();
	const auto Data = Prepared(Asset);
	const auto Handle = Scene.Add(Model(Data));
	const FRay Ray{{0, 0, 5}, {0, 0, -1}, 0, 10};
	for (const auto ScaleValue : {FVec3{2, .5f, 3}, FVec3{-2, .5f, 3}, FVec3{1, 1, 0}})
	{
		auto World = Multiply(Translation({0, 0, 1}), Scale(ScaleValue));
		World.Values[4] = .2f;
		Scene.SetWorldTransform(Handle, World);
		const auto Hit = Scene.Raycast(Ray);
		HYP_CHECK(Hit.Status == ESceneRayStatus::Hit && std::abs(Hit.Distance - 4) < .0001f);
	}
	Scene.SetWorldTransform(Handle, Identity());
	HYP_CHECK(Scene.Raycast({Ray.Origin, Ray.Direction, 0, 4}).Status == ESceneRayStatus::Miss);
	HYP_CHECK(Scene.Raycast({Ray.Origin, Ray.Direction, 6, 10}).Status == ESceneRayStatus::Miss);
	HYP_CHECK(Scene.Raycast({{}, {}, 0, 10}).Status == ESceneRayStatus::Unavailable);
	auto Pending = Model(PrepareSceneModel(Asset));
	const auto Missing = Scene.Add(Pending);
	auto Hit = Scene.Raycast(Ray);
	HYP_CHECK(Hit.Status == ESceneRayStatus::Hit && Hit.bIncomplete);
	Scene.SetModelVisible(Handle, false);
	HYP_CHECK(Scene.Raycast(Ray).Status == ESceneRayStatus::Unavailable);
	Scene.Update(Missing, Model(Data));
	HYP_CHECK(Scene.Raycast(Ray).Handle == Missing);
	Scene.Remove(Missing);
	HYP_CHECK(Scene.Raycast(Ray).Status == ESceneRayStatus::Miss);
	unsigned Checks{};
	try
	{
		PrepareSceneModelGeometry(*Mesh(1000),
		                          [&]
		                          {
			                          if (++Checks == 3)
			                          {
				                          throw std::runtime_error("cancel");
			                          }
		                          });
		HYP_CHECK(false);
	}
	catch (const std::runtime_error&)
	{
		HYP_CHECK(Checks == 3);
	}
}

void CheckRoundedTies()
{
	for (const float Z : {0.f, .1f})
	{
		auto FirstAsset = Mesh();
		for (std::size_t Index = 2; Index < FirstAsset->Primitives[0].Positions.size(); Index += 3)
		{
			FirstAsset->Primitives[0].Positions[Index] = Z;
		}
		auto SecondAsset = std::make_shared<FModelAsset>(*FirstAsset);
		auto& Primitive = SecondAsset->Primitives[0];
		// A non-intersecting triangle brings the second object's bounds closer.
		Primitive.Positions.insert(Primitive.Positions.end(), {3, -1, .5f, 5, -1, .5f, 4, 1, .5f});
		Primitive.Indices.insert(Primitive.Indices.end(), {3, 4, 5});
		FScene Scene;
		const auto First = Scene.Add(Model(Prepared(FirstAsset)));
		Scene.Add(Model(Prepared(SecondAsset)));
		const FRay Ray{{0, 0, 1}, {0, 0, -1}, 0, 10};
		const auto Hit = Scene.Raycast(Ray);
		HYP_CHECK(Hit.Handle == First && Hit.Stats.Bounds.CandidateGroups == 2);
		if (Z != 0)
		{
			HYP_CHECK(double(Hit.Distance) < 1.0 - double(Z));
			HYP_CHECK(Scene.Raycast({Ray.Origin, Ray.Direction, 0, Hit.Distance}).Status == ESceneRayStatus::Miss);
			const float Beyond = std::nextafter(Hit.Distance, 10.f);
			HYP_CHECK(Scene.Raycast({Ray.Origin, Ray.Direction, Beyond, 10}).Status == ESceneRayStatus::Miss);
		}
	}
	// Put the higher-index equal-distance triangle in the first BVH child.
	auto Asset = Mesh(8);
	auto& Primitive = Asset->Primitives[0];
	for (unsigned Triangle = 0; Triangle < 8; ++Triangle)
	{
		for (unsigned Corner = 0; Corner < 3; ++Corner)
		{
			const auto Offset = Triangle * 9 + Corner * 3;
			Primitive.Positions[Offset] -= 5 * float(Triangle);
			Primitive.Positions[Offset + 2] = .1f;
		}
	}
	Primitive.Positions.insert(Primitive.Positions.end(), {-20, -1, .1f, 1, -1, .1f, 0, 1, .1f});
	Primitive.Indices.insert(Primitive.Indices.end(), {24, 25, 26});
	FScene Scene;
	Scene.Add(Model(Prepared(Asset)));
	const auto Hit = Scene.Raycast({{0, 0, 1}, {0, 0, -1}, 0, 10});
	HYP_CHECK(Hit.Status == ESceneRayStatus::Hit && Hit.Triangle == 0);
}

void CheckScheduledMaterialPasses()
{
	struct FCase
	{
		std::vector<std::string> Usages;
		bool bDeferred{};
		bool bForward{};
	};

	const std::vector<FCase> Cases{{{"Forward"}, true, true},
	                               {{"HdrForwardOpaque", "Forward"}, false, true},
	                               {{"DeferredBase", "Forward"}, true, false},
	                               {{"HdrCompatibility", "Forward"}, true, false},
	                               {{"HdrTransparent", "Forward"}, true, true},
	                               {{"HdrForwardOpaque", "DeferredBase", "Forward"}, true, true},
	                               {{"ShadowDepth", "Forward"}, true, true},
	                               {{"ShadowDepth"}, false, false}};
	const auto Data = Prepared(Mesh());
	for (const auto& Test : Cases)
	{
		FMaterialDescription Description;
		Description.Name = "scheduled pass policy";
		for (const auto& Usage : Test.Usages)
		{
			FMaterialPass Pass;
			Pass.Usage = Usage;
			Pass.Vertex = {"Test.hlsl", "VsMain"};
			Pass.Pixel = {"Test.hlsl", "PsMain"};
			Description.Passes.push_back(Pass);
		}
		FScene Scene;
		auto Value = Model(Data);
		Value.Surface.Instance =
		    std::make_shared<FMaterialInstance>(std::make_shared<FMaterialDefinition>(Description));
		Scene.Add(Value);
		const FRay Ray{{0, 0, 1}, {0, 0, -1}, 0, 10};
		HYP_CHECK((Scene.Raycast(Ray, MakeSceneRayOptions(ESceneRenderPipeline::Deferred)).Status ==
		           ESceneRayStatus::Hit) == Test.bDeferred);
		HYP_CHECK((Scene.Raycast(Ray, MakeSceneRayOptions(ESceneRenderPipeline::Forward)).Status ==
		           ESceneRayStatus::Hit) == Test.bForward);
	}
}

void CheckMaterialFaces()
{
	FScene Scene;
	auto Data = Prepared(Mesh());
	FMaterialDescription Description;
	Description.Name = "face policy";
	FMaterialPass Pass;
	Pass.Vertex = {"Test.hlsl", "VsMain"};
	Pass.Pixel = {"Test.hlsl", "PsMain"};
	Pass.State.Cull = EMaterialCull::Back;
	Description.Passes = {Pass};
	auto Material = std::make_shared<FMaterialInstance>(std::make_shared<FMaterialDefinition>(Description));
	auto Value = Model(Data);
	Value.Surface.Instance = Material;
	const auto Handle = Scene.Add(Value);
	const FRay Front{{0, 0, 5}, {0, 0, -1}, 0, 10};
	const FRay Back{{0, 0, -5}, {0, 0, 1}, 0, 10};
	HYP_CHECK(Scene.Raycast(Front).Status == ESceneRayStatus::Hit);
	HYP_CHECK(Scene.Raycast(Back).Status == ESceneRayStatus::Miss);
	HYP_CHECK(Scene.Raycast(Back, {true}).Status == ESceneRayStatus::Hit);
	Scene.SetWorldTransform(Handle, Scale({-1, 1, 1}));
	HYP_CHECK(Scene.Raycast(Front).Status == ESceneRayStatus::Hit);
	Description.Passes.front().State.Cull = EMaterialCull::Front;
	FPreparedMaterialInterface Interface;
	Interface.Definition = std::make_shared<FMaterialDefinition>(Description);
	Interface.Schema = std::make_shared<const FMaterialParameterSchema>(Description.Parameters, Description.Version);
	Material->ReplaceDefinition(Interface);
	auto Hit = Scene.Raycast(Front);
	HYP_CHECK(Hit.Status == ESceneRayStatus::Miss && Hit.Stats.Bounds.IndexRebuilds == 0);
	HYP_CHECK(Scene.Raycast(Back).Status == ESceneRayStatus::Hit);
	HYP_CHECK(Scene.Raycast(Back, {false, {"UnscheduledPass"}}).Status == ESceneRayStatus::Miss);
	auto ValueWithSection = *Scene.Find(Handle);
	ValueWithSection.SectionSurfaces[0].Instance = std::make_shared<FMaterialInstance>(
	    std::make_shared<FMaterialDefinition>(FMaterialDescription{"section override", 1, {Pass}}));
	Scene.Update(Handle, ValueWithSection);
	HYP_CHECK(Scene.Raycast(Front).Status == ESceneRayStatus::Hit);
	HYP_CHECK(Scene.Raycast(Front).Stats.Bounds.IndexRefits == 0);
}

void CheckAcceleration()
{
	const auto Data = Prepared(Mesh(4096));
	FScene Scene;
	const auto Handle = Scene.Add(Model(Data));
	std::mt19937 Random(7123);
	std::uniform_real_distribution<float> X(-1, 4096 * 4.f);
	std::uniform_real_distribution<float> Y(-1.2f, 1.2f);
	for (unsigned Trial = 0; Trial < 150; ++Trial)
	{
		const FRay Ray{{X(Random), Y(Random), 3}, {0, 0, -1}, 0, 10};
		bool bOracle{};
		for (unsigned Triangle = 0; Triangle < 4096; ++Triangle)
		{
			const float Center = float(Triangle) * 4;
			bOracle |= IntersectRayTriangle(Ray, {Center - 1, -1, 0}, {Center + 1, -1, 0}, {Center, 1, 0}).has_value();
		}
		const auto Hit = Scene.Raycast(Ray);
		HYP_CHECK((Hit.Status == ESceneRayStatus::Hit) == bOracle);
		HYP_CHECK(Hit.Stats.TriangleTests <= 8);
	}
	HYP_CHECK(Data->QueryGeometry->StorageBytes < 4096 * 32);
	Scene.Remove(Handle);
	const auto Small = Prepared(Mesh());
	for (unsigned Index = 0; Index < 1024; ++Index)
	{
		Scene.Add(Model(Small, {float(Index) * 4, 0, 0}));
	}
	const auto Hit = Scene.Raycast({{0, 0, 4}, {0, 0, -1}, 0, 10});
	HYP_CHECK(Hit.Status == ESceneRayStatus::Hit && Hit.Stats.Bounds.CandidateGroups == 1);
	HYP_CHECK(Hit.Stats.Bounds.VisitedNodes < 40 && Hit.Stats.TriangleTests == 1);
	std::cout << "4096 triangles: <=8 exact tests; 1024 objects: " << Hit.Stats.Bounds.CandidateGroups << " candidate, "
	          << Hit.Stats.Bounds.VisitedNodes << " nodes; compact bytes: " << Data->QueryGeometry->StorageBytes
	          << '\n';
}

void CheckViewportRays()
{
	FSceneCameraView Camera;
	Camera.World = SceneCameraTransform({2, 3, 5}, {0, 0, 0});
	const auto Pose = ExtractScenePose(Camera.World);
	for (const auto Depth : {EDepthConvention::Standard, EDepthConvention::Reversed})
	{
		for (const auto Position : {FVec2{.5f, .5f}, FVec2{.1f, .2f}, FVec2{.99f, .99f}})
		{
			const auto Ray = MakeViewportRay(Camera, Position, 1513, 793, Depth);
			HYP_CHECK(Ray);
			const auto World = Add(Ray->Origin, ScaleVector(Ray->Direction, 10));
			const auto Clip = Transform(SceneCameraViewProjection(Pose, Camera.Lens, 1513.f / 793, Depth),
			                            {World.X, World.Y, World.Z, 1});
			HYP_CHECK(std::abs((Clip.X / Clip.W + 1) / 2 - Position.X) < .0001f);
			HYP_CHECK(std::abs((1 - Clip.Y / Clip.W) / 2 - Position.Y) < .0001f);
			HYP_CHECK(std::abs(Ray->Minimum * Dot(Ray->Direction, Pose.Forward) - Camera.Lens.Near) < .00001f);
			HYP_CHECK(std::abs(Ray->Maximum * Dot(Ray->Direction, Pose.Forward) - Camera.Lens.Far) < .001f);
		}
	}
	HYP_CHECK(!MakeViewportRay(Camera, {.5f, .5f}, 0, 100));
	HYP_CHECK(!MakeViewportRay(Camera, {2, .5f}, 100, 100));
	Camera.World = Scale({0, 0, 0});
	HYP_CHECK(!MakeViewportRay(Camera, {.5f, .5f}, 100, 100));
}
} // namespace

int main()
{
	try
	{
		CheckQueries();
		CheckTransformsAndReadiness();
		CheckRoundedTies();
		CheckScheduledMaterialPasses();
		CheckMaterialFaces();
		CheckAcceleration();
		CheckViewportRays();
		std::cout << "PASS: scene ray queries and viewport rays\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
