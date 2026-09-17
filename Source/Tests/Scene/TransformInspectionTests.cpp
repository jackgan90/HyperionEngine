#include "Hyperion/Math/AffineTransform.h"
#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace
{
using namespace Hyperion;
constexpr float HalfPi = std::numbers::pi_v<float> / 2;

void Near(const FMat4& InA, const FMat4& InB)
{
	for (unsigned Index = 0; Index < 16; ++Index)
	{
		HYP_CHECK(std::abs(InA.Values[Index] - InB.Values[Index]) <=
		          2e-5f * std::max(1.f, std::abs(InA.Values[Index])));
	}
}

void CheckRotationAxes()
{
	const auto X = Transform(ComposeAffine({.Rotation = {HalfPi, 0, 0}}), {0, 1, 0, 0});
	const auto Y = Transform(ComposeAffine({.Rotation = {0, HalfPi, 0}}), {0, 0, 1, 0});
	const auto Z = Transform(ComposeAffine({.Rotation = {0, 0, HalfPi}}), {1, 0, 0, 0});
	HYP_CHECK(std::abs(X.Z - 1) < 1e-6f && std::abs(X.Y) < 1e-6f);
	HYP_CHECK(std::abs(Y.X - 1) < 1e-6f && std::abs(Y.Z) < 1e-6f);
	HYP_CHECK(std::abs(Z.Y - 1) < 1e-6f && std::abs(Z.X) < 1e-6f);
	const auto Ordered =
	    Multiply(ComposeAffine({.Rotation = {0, 0, .7f}}),
	             Multiply(ComposeAffine({.Rotation = {0, .4f, 0}}), ComposeAffine({.Rotation = {.2f, 0, 0}})));
	Near(Ordered, ComposeAffine({.Rotation = {.2f, .4f, .7f}}));
}

void CheckAffineRoundTrips()
{
	std::mt19937 Random(1879);
	std::uniform_real_distribution<float> Value(-3.f, 3.f);
	for (unsigned Index = 0; Index < 500; ++Index)
	{
		FAffineTransform Input{{Value(Random), Value(Random), Value(Random)},
		                       {Value(Random), Value(Random), Value(Random)},
		                       {Value(Random), Value(Random), Value(Random)},
		                       {Value(Random), Value(Random), Value(Random)}};
		if (Index % 5 == 0)
		{
			Input.Rotation.Y = Index % 2 ? HalfPi : -HalfPi;
		}
		if (Index % 7 == 0)
		{
			Input.Scale.X = 0;
		}
		if (Index % 11 == 0)
		{
			Input.Scale.Y = Input.Scale.Z = 0;
		}
		const auto Matrix = ComposeAffine(Input);
		Near(Matrix, ComposeAffine(DecomposeAffine(Matrix)));
	}
	for (const auto ScaleValue : {FVec3{0, 0, 0}, FVec3{0, 1, 1}, FVec3{1, 0, 1}, FVec3{1, 1, 0}, FVec3{-2, 3, 4},
	                              FVec3{2, -3, 4}, FVec3{2, 3, -4}})
	{
		const auto Matrix = Scale(ScaleValue);
		Near(Matrix, ComposeAffine(DecomposeAffine(Matrix)));
	}
}

void CheckTransformDraft()
{
	FSceneTransform Source;
	Source.Local = ComposeAffine({{7, 8, 9}, {.2f, -.6f, .8f}, {-2, 3, 4}, {.3f, -.4f, .8f}});
	const auto Original = Source.Local.Values;
	FRecordDraft Draft(RecordType<FSceneTransform>(), &Source);
	HYP_CHECK(Draft.GetValues().contains("position") && Draft.GetValues().contains("rotation"));
	HYP_CHECK(!Draft.GetValues().contains("local"));
	auto Candidate = Source;
	Draft.ApplyToCandidate(&Candidate);
	HYP_CHECK(Candidate.Local.Values == Original);
	Draft.GetValues().at("parent") = WriteValue(std::string("parent"));
	Draft.ApplyToCandidate(&Candidate);
	HYP_CHECK(Candidate.Local.Values == Original && Candidate.Parent == "parent");
	Draft.GetValues().at("position") = WriteValue(FVec3{11, 12, 13});
	Draft.ApplyToCandidate(&Candidate);
	HYP_CHECK(std::equal(Original.begin(), Original.begin() + 12, Candidate.Local.Values.begin()));
	HYP_CHECK(Candidate.Local.Values[12] == 11 && Candidate.Local.Values[14] == 13);
	Draft.GetValues().at("rotation") = WriteValue(FVec3{30, 90, 70});
	Draft.GetValues().at("scale") = WriteValue(FVec3{-5, 0, 2});
	Draft.ApplyToCandidate(&Candidate);
	auto Expected = DecomposeAffine(Source.Local);
	Expected.Position = {11, 12, 13};
	Expected.Rotation = ScaleVector({30, 90, 70}, std::numbers::pi_v<float> / 180);
	Expected.Scale = {-5, 0, 2};
	Near(Candidate.Local, ComposeAffine(Expected));
	HYP_CHECK(Source.Local.Values == Original);
	const auto Reloaded = ReadValue<FSceneTransform>(WriteValue(Candidate));
	HYP_CHECK(Reloaded.Local.Values == Candidate.Local.Values && Reloaded.Parent == "parent");
}

void CheckSceneTransaction()
{
	FScene Scene;
	FSceneNode Node;
	Node.Id = "edited";
	const auto Handle = Scene.AddNode(Node);
	const auto Revision = Scene.GetRevision();
	FRecordDraft Draft(RecordType<FSceneTransform>(), &*Node.Components.Slot<FSceneTransform>());
	Draft.GetValues().at("rotation") = WriteValue(FVec3{0, 0, 90});
	Draft.ApplyToCandidate(&*Node.Components.Slot<FSceneTransform>());
	HYP_CHECK(Scene.EditNode(Handle, Node, Revision));
	HYP_CHECK(!Scene.EditNode(Handle, Node, Revision));
	const auto Point = Transform(Scene.FindNode(Handle)->Local(), {1, 0, 0, 0});
	HYP_CHECK(std::abs(Point.Y - 1) < 1e-6f && std::abs(Point.X) < 1e-6f);
	const auto Entry = SceneEntryFromNode(*Scene.FindNode(Handle));
	HYP_CHECK(NodeFromSceneEntry(ReadValue<FSceneNodeEntry>(WriteValue(Entry))).Local().Values == Node.Local().Values);
}
} // namespace

void CheckTransformInspection()
{
	CheckRotationAxes();
	CheckAffineRoundTrips();
	CheckTransformDraft();
	CheckSceneTransaction();
}
