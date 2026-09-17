#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"

namespace Hyperion
{
struct FTestComponent
{
	double Weight = 1;
	std::vector<std::string> Tags;
	bool operator==(const FTestComponent&) const = default;
};

struct FRepeatedTestComponent
{
	std::string Value;
	bool operator==(const FRepeatedTestComponent&) const = default;
};

template<> const FRecordDescriptor& RecordType<FRepeatedTestComponent>()
{
	static const auto Type = MakeRecord<FRepeatedTestComponent>(
	    "test.repeated-component", {Member("value", &FRepeatedTestComponent::Value, Inspect("Value"))});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FTestComponent>()
{
	static const auto Type =
	    MakeRecord<FTestComponent>("test.component",
	                               {Member("weight", &FTestComponent::Weight, Inspect("Weight", 0, 10)),
	                                Member("tags", &FTestComponent::Tags, Inspect("Tags"))},
	                               1,
	                               [](const FTestComponent& InValue)
	                               {
		                               if (InValue.Weight < 0)
		                               {
			                               throw std::invalid_argument("Negative test component weight");
		                               }
	                               });
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

template<class T> void Reject(const T& InOperation)
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

void CheckComposition()
{
	FScene Scene;
	auto Node = MakeSceneCameraNode("composed");
	Node.PointLight() = FScenePointLight{};
	Node.Components.Add("custom", "test.component");
	const auto Handle = Scene.AddNode(Node);
	Scene.SetSettings({Handle, {}, {}});
	HYP_CHECK(Scene.CountNodes(ESceneNodeKind::Camera) == 1);
	HYP_CHECK(Scene.CountNodes(ESceneNodeKind::PointLight) == 1);
	HYP_CHECK(Scene.GetNodes(ESceneNodeKind::PointLight) == std::vector<FSceneHandle>{Handle});
	const auto Revision = Scene.GetRevision();
	Node.Components.Slot<FTestComponent>()->Weight = -1;
	Reject(
	    [&]
	    {
		    Scene.EditNode(Handle, Node, Revision);
	    });
	HYP_CHECK(Scene.GetRevision() == Revision);
	HYP_CHECK(Scene.FindNode(Handle)->Components.Slot<FTestComponent>()->Weight == 1);
	Node.Components.Slot<FTestComponent>()->Weight = 3;
	HYP_CHECK(!Scene.EditNode(Handle, Node, Revision - 1));
	HYP_CHECK(Scene.EditNode(Handle, Node, Revision));
	HYP_CHECK(Scene.FindNode(Handle)->Components.Slot<FTestComponent>()->Weight == 3);
	Node.Camera().reset();
	HYP_CHECK(Scene.EditNode(Handle, Node, Scene.GetRevision()));
	HYP_CHECK(!Scene.GetSettings().DefaultCamera && Scene.CountNodes(ESceneNodeKind::Camera) == 0);
	Reject(
	    [&]
	    {
		    Node.Components.Remove(RecordType<FSceneTransform>().Id);
	    });
	Reject(
	    [&]
	    {
		    Node.Components.Add("duplicate", "test.component");
	    });
	Scene.RemoveSubtree(Handle);
	HYP_CHECK(!Scene.EditNode(Handle, Node, Scene.GetRevision()));
}

void CheckComponentArchive()
{
	auto Node = MakeSceneCameraNode("composed");
	Node.PointLight() = FScenePointLight{};
	Node.Components.Add("extension-instance", "test.component");
	Node.Components.Slot<FTestComponent>()->Tags = {"first", "second"};
	Node.Components.Rename(RecordType<FSceneCamera>().Id, "lens-instance");
	const auto Entry = SceneEntryFromNode(Node);
	const auto Archive = WriteValue(Entry);
	const auto Restored = NodeFromSceneEntry(ReadValue<FSceneNodeEntry>(Archive));
	HYP_CHECK(Restored == Node);
	HYP_CHECK(Restored.Components.Find("extension-instance"));
	HYP_CHECK(Restored.Components.Find("lens-instance"));
	const auto& Type = RecordType<FTestComponent>();
	HYP_CHECK(Type.Members[0].Options.Inspector->Label == "Weight");
	HYP_CHECK(Type.Members[1].Shape().Kind == ERecordValueKind::Sequence);
	HYP_CHECK(Type.Members[1].Shape().Element->Kind == ERecordValueKind::String);
	FRecordDraft Draft(Type, Restored.Components.Find("extension-instance")->Get());
	Draft.GetValues().at("weight") = WriteValue(-1.0);
	auto Candidate = Restored;
	Reject(
	    [&]
	    {
		    Draft.ApplyToCandidate(Candidate.Components.Find("extension-instance")->Edit());
	    });
	HYP_CHECK(Restored.Components.Slot<FTestComponent>()->Weight == 1);
	Draft.GetValues().at("weight") = WriteValue(2.0);
	Draft.ApplyToCandidate(Candidate.Components.Find("extension-instance")->Edit());
	HYP_CHECK(Candidate.Components.Slot<FTestComponent>()->Weight == 2);
	// The type validator only rejects negative values; this exercises the presentation upper bound.
	Draft.GetValues().at("weight") = WriteValue(11.0);
	Reject(
	    [&]
	    {
		    Draft.ApplyToCandidate(Candidate.Components.Find("extension-instance")->Edit());
	    });
	FSceneMeshSection Section{"stable-primitive"};
	FRecordDraft SectionDraft(RecordType<FSceneMeshSection>(), &Section);
	SectionDraft.GetValues().at("primitive") = WriteValue(std::string("forged-identity"));
	SectionDraft.GetValues().at("visible") = WriteValue(false);
	SectionDraft.ApplyToCandidate(&Section);
	HYP_CHECK(Section.Primitive == "stable-primitive" && !Section.bVisible);
	auto Unknown = Archive;
	auto& Fields = std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Unknown.Value).at("fields").Value);
	auto& Components = std::get<FArchiveNode::FArray>(Fields.at("components").Value);
	auto& Extension = std::get<FArchiveNode::FObject>(Components.back().Value);
	Extension.at("type") = WriteValue(std::string("missing.plugin"));
	const auto Unsupported = ReadValue<FSceneNodeEntry>(Unknown);
	HYP_CHECK(NodeFromSceneEntry(Unsupported).Components.Unknown().size() == 1);
	Reject(
	    [&]
	    {
		    WriteValue(Unsupported);
	    });
}

void CheckComponentIdentityRoundTrip()
{
	auto Node = MakeSceneCameraNode("identity-permutation");
	Node.PointLight() = FScenePointLight{};
	const auto CameraId = RecordType<FSceneCamera>().Id;
	const auto PointId = RecordType<FScenePointLight>().Id;
	const auto TransformId = RecordType<FSceneTransform>().Id;
	const auto CheckRoundTrip = [&]
	{
		ValidateSceneNode(Node);
		const auto Entry = SceneEntryFromNode(Node);
		HYP_CHECK(NodeFromSceneEntry(Entry) == Node);
		HYP_CHECK(NodeFromSceneEntry(ReadValue<FSceneNodeEntry>(WriteValue(Entry))) == Node);
	};
	Node.Components.Rename(CameraId, "temporary");
	Node.Components.Rename(PointId, CameraId);
	Node.Components.Rename("temporary", PointId);
	CheckRoundTrip();
	Node.Components.Rename(TransformId, "temporary");
	Node.Components.Rename(PointId, TransformId);
	Node.Components.Rename("temporary", PointId);
	CheckRoundTrip();
	Node.Components.Rename(CameraId, "light-instance");
	Node.Components.Add(CameraId, RecordType<FTestComponent>().Id);
	Node.Components.Slot<FTestComponent>()->Tags = {"type IDs are valid instance IDs"};
	CheckRoundTrip();
}

void CheckRepeatedComponents()
{
	FSceneNode Node;
	Node.Id = Node.Name = "multiple";
	const auto& Type = RecordType<FRepeatedTestComponent>().Id;
	Node.Components.Add("first-instance", Type);
	Node.Components.Add(Type, Type);
	static_cast<FRepeatedTestComponent*>(Node.Components.Find("first-instance")->Edit())->Value =
	    "first retained value";
	static_cast<FRepeatedTestComponent*>(Node.Components.Find(Type)->Edit())->Value = "second retained value";
	const auto Entry = SceneEntryFromNode(Node);
	HYP_CHECK(Entry.Extensions.Find("first-instance") && Entry.Extensions.Find(Type));
	const auto Restored = NodeFromSceneEntry(ReadValue<FSceneNodeEntry>(WriteValue(Entry)));
	HYP_CHECK(Restored == Node);
	HYP_CHECK(NodeFromSceneEntry(Entry) == Node);
}
} // namespace

void CheckSceneComponents()
{
	SceneComponentRegistry().Register(MakeSceneComponent<FTestComponent>("Test component"));
	auto Repeated = MakeSceneComponent<FRepeatedTestComponent>("Repeated test component");
	Repeated.bUnique = false;
	SceneComponentRegistry().Register(std::move(Repeated));
	CheckComposition();
	CheckComponentArchive();
	CheckComponentIdentityRoundTrip();
	CheckRepeatedComponents();
}
