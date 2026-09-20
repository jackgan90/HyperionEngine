#include "Hyperion/Math/AffineTransform.h"
#include "Hyperion/Scene/Scene.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <limits>

namespace
{
using namespace Hyperion;

template<class T> void RejectBatch(const T& InOperation)
{
	bool bRejected{};
	try
	{
		InOperation();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

FSceneNode Node(std::string InId, std::string InParent = {})
{
	FSceneNode Result;
	Result.Id = std::move(InId);
	Result.Parent() = std::move(InParent);
	return Result;
}

void CheckAtomicHierarchy()
{
	FScene Scene;
	const auto Handles = Scene.AddNodes({Node("child", "parent"), Node("parent"), Node("other")});
	auto Child = *Scene.FindNode(Handles[0]);
	auto Parent = *Scene.FindNode(Handles[1]);
	const auto Revision = Scene.GetRevision();
	Child.Local() = Translation({15, 2, 3});
	Parent.Local() = Translation({15, 4, 5});
	auto Invalid = Parent;
	Invalid.Local().Values[0] = std::numeric_limits<float>::infinity();
	RejectBatch(
	    [&]
	    {
		    Scene.EditNodes({{Handles[0], Child}, {Handles[1], Invalid}}, Revision);
	    });
	HYP_CHECK(Scene.GetRevision() == Revision && Scene.FindNode(Handles[0])->Local().Values[12] == 0);
	HYP_CHECK(!Scene.EditNodes({{Handles[0], Child}}, Revision - 1));
	HYP_CHECK(Scene.EditNodes({{Handles[0], Child}, {Handles[1], Parent}}, Revision));
	HYP_CHECK(Scene.GetRevision() == Revision + 1);
	FMat4 World;
	HYP_CHECK(Scene.GetWorld(Handles[0], World) && World.Values[12] == 30);
	// Only the final hierarchy matters: the former child becomes the parent in one batch.
	Parent.Parent() = "child";
	Child.Parent().clear();
	HYP_CHECK(Scene.EditNodes({{Handles[1], Parent}, {Handles[0], Child}}, Scene.GetRevision()));
	const auto BeforeCycle = Scene.GetRevision();
	Child.Parent() = "parent";
	RejectBatch(
	    [&]
	    {
		    Scene.EditNodes({{Handles[0], Child}}, BeforeCycle);
	    });
	HYP_CHECK(Scene.GetRevision() == BeforeCycle && Scene.FindNode(Handles[0])->Parent().empty());
	const auto BeforeDelete = Scene.GetRevision();
	HYP_CHECK(Scene.RemoveSubtrees(Handles));
	HYP_CHECK(Scene.CountNodes() == 0 && Scene.GetRevision() == BeforeDelete + 1);
	Child.Parent().clear();
	const auto Restored = Scene.AddNodes({Parent, Child, Node("other")});
	HYP_CHECK(Scene.CountNodes() == 3 && !Scene.FindNode(Handles[0]));
	HYP_CHECK(Scene.FindNode(Restored[0])->Parent() == "child");
	const auto BeforeBadAdd = Scene.GetRevision();
	RejectBatch(
	    [&]
	    {
		    Scene.AddNodes({Node("valid"), Node("invalid", "missing")});
	    });
	HYP_CHECK(Scene.GetRevision() == BeforeBadAdd && !Scene.FindNode("valid"));
	const std::array BadRemoval{Restored[0], Handles[0]};
	HYP_CHECK(!Scene.RemoveSubtrees(BadRemoval) && Scene.FindNode(Restored[0]));
}

void CheckMixedTransforms()
{
	FSceneTransform A;
	FSceneTransform B;
	A.Local = ComposeAffine({{10, 2, 3}, {}, {1, 2, 3}, {.2f, .3f, .4f}});
	B.Local = ComposeAffine({{20, 8, 9}, {}, {4, 5, 6}, {.5f, .6f, .7f}});
	const auto OriginalB = B;
	const std::array<const void*, 2> Values{&A, &B};
	FRecordSelectionDraft Draft(RecordType<FSceneTransform>(), Values);
	const FInspectionPath X{"position", "fields", "x"};
	HYP_CHECK(Draft.IsMixed(X));
	// Explicitly writing the primary value must still replace the secondary value.
	Draft.SetValue(X, WriteValue(10.f));
	Draft.ApplyToCandidate(0, &A);
	Draft.ApplyToCandidate(1, &B);
	HYP_CHECK(A.Local.Values[12] == 10 && B.Local.Values[12] == 10);
	HYP_CHECK(B.Local.Values[13] == 8 && B.Local.Values[14] == 9);
	for (unsigned Index = 0; Index < 12; ++Index)
	{
		HYP_CHECK(B.Local.Values[Index] == OriginalB.Local.Values[Index]);
	}
	HYP_CHECK(!Draft.IsMixed(X));
}

void CheckDeepBatchHierarchy()
{
	FScene Scene;
	std::vector<FSceneNode> Nodes;
	constexpr unsigned Count = 1024;
	for (unsigned Index = 0; Index < Count; ++Index)
	{
		Nodes.push_back(Node("deep-" + std::to_string(Index), Index ? "deep-" + std::to_string(Index - 1) : ""));
	}
	const auto Handles = Scene.AddNodes(std::move(Nodes));
	std::vector<FSceneNodeEdit> Edits;
	for (std::size_t Index = Handles.size(); Index-- > 0;)
	{
		auto Candidate = *Scene.FindNode(Handles[Index]);
		Candidate.Local() = Translation({1, 0, 0});
		Edits.push_back({Handles[Index], std::move(Candidate)});
	}
	HYP_CHECK(Scene.EditNodes(std::move(Edits), Scene.GetRevision()));
	FMat4 World;
	HYP_CHECK(Scene.GetWorld(Handles.back(), World) && World.Values[12] == Count);
	auto Root = *Scene.FindNode(Handles.front());
	auto Leaf = *Scene.FindNode(Handles.back());
	Root.bEnabled = false;
	Leaf.Local() = Translation({2, 0, 0});
	HYP_CHECK(Scene.EditNodes({{Handles.back(), Leaf}, {Handles.front(), Root}}, Scene.GetRevision()));
	FSceneNodeView View;
	HYP_CHECK(Scene.GetNodeView(Handles.back(), View) && !View.bEffectiveEnabled && View.World.Values[12] == Count + 1);
	const auto Revision = Scene.GetRevision();
	Root.Parent() = Leaf.Id;
	RejectBatch(
	    [&]
	    {
		    Scene.EditNodes({{Handles.front(), Root}}, Revision);
	    });
	HYP_CHECK(Scene.GetRevision() == Revision && Scene.FindNode(Handles.front())->Parent().empty());
	RejectBatch(
	    [&]
	    {
		    Scene.AddNodes({Node("cycle-a", "cycle-b"), Node("cycle-b", "cycle-a")});
	    });
	HYP_CHECK(Scene.GetRevision() == Revision && !Scene.FindNode("cycle-a"));
	// Disjoint candidate leaves share a long, unchanged ancestor chain.
	const auto Branches = Scene.AddNodes({Node("branch-a", Leaf.Id), Node("branch-b", Leaf.Id)});
	auto A = *Scene.FindNode(Branches[0]);
	auto B = *Scene.FindNode(Branches[1]);
	A.Local() = Translation({3, 0, 0});
	B.Local() = Translation({4, 0, 0});
	HYP_CHECK(Scene.EditNodes({{Branches[1], B}, {Branches[0], A}}, Scene.GetRevision()));
	HYP_CHECK(Scene.GetWorld(Branches[0], World) && World.Values[12] == Count + 4);
	HYP_CHECK(Scene.GetWorld(Branches[1], World) && World.Values[12] == Count + 5);
}

void CheckMixedOptionalsAndCollections()
{
	FSceneModelComponent A;
	FSceneModelComponent B;
	A.Material.Roughness = .2f;
	B.Material.Roughness = .8f;
	A.Material.Metallic = .4f;
	A.Sections.push_back({"one"});
	B.Sections.push_back({"two"});
	const std::array<const void*, 2> Values{&A, &B};
	FRecordSelectionDraft Draft(RecordType<FSceneModelComponent>(), Values);
	const FInspectionPath Metal{"material", "fields", "metallic"};
	HYP_CHECK(Draft.IsPresenceMixed(Metal));
	FRecordValueShape Shape;
	for (const auto& Member : RecordType<FMaterialOverride>().Members)
	{
		if (Member.Id == "metallic")
		{
			Shape = Member.Shape();
		}
	}
	Draft.SetPresent(Metal, Shape, true);
	HYP_CHECK(!Draft.IsPresenceMixed(Metal) && Draft.IsMixed(Metal));
	Draft.SetValue(Metal, WriteValue(.6f));
	Draft.ApplyToCandidate(0, &A);
	Draft.ApplyToCandidate(1, &B);
	HYP_CHECK(A.Material.Metallic == .6f && B.Material.Metallic == .6f);
	HYP_CHECK(A.Material.Roughness == .2f && B.Material.Roughness == .8f);
	HYP_CHECK(!Draft.CanEditCollection({"sections"}, {.ElementIdentity = "primitive"}));
	B.Sections[0].Primitive = "one";
	B.Sections[0].bVisible = false;
	FRecordSelectionDraft Compatible(RecordType<FSceneModelComponent>(), Values);
	HYP_CHECK(Compatible.CanEditCollection({"sections"}, {.ElementIdentity = "primitive"}));
	HYP_CHECK(Compatible.IsMixed({"sections", "0", "fields", "visible"}));
	Compatible.SetValue({"sections", "0", "fields", "primitive"}, WriteValue(std::string("updated")));
	HYP_CHECK(Compatible.CanEditCollection({"sections"}, {.ElementIdentity = "primitive"}));
	HYP_CHECK(!Compatible.IsMixed({"sections", "0", "fields", "primitive"}));
	HYP_CHECK(EqualInspectionValue(WriteValue(0.f), WriteValue(-0.f)));
	HYP_CHECK(!EqualInspectionValue(WriteValue(1.f), WriteValue(std::nextafter(1.f, 2.f))));
}
} // namespace

void CheckSceneBatches()
{
	CheckAtomicHierarchy();
	CheckDeepBatchHierarchy();
	CheckMixedTransforms();
	CheckMixedOptionalsAndCollections();
}
