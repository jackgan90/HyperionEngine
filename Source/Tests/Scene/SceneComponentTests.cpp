#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <limits>
#include <numbers>

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
	Scene.SetSettings({Handle, {}});
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

FArchiveNode LegacyEnvironmentLight(std::uint32_t InVersion)
{
	FSceneEnvironmentLight Light;
	auto Node = WriteRecord(RecordType<FSceneEnvironmentLight>(), &Light);
	auto& Object = std::get<FArchiveNode::FObject>(Node.Value);
	Object.at("version") = WriteValue(InVersion);
	auto& Fields = std::get<FArchiveNode::FObject>(Object.at("fields").Value);
	Fields.erase("tint");
	Fields.erase("yawDegrees");
	Fields["yawRadians"] = WriteValue(std::numbers::pi / 2);
	Fields["sky"] = FArchiveNode(std::monostate{});
	if (InVersion == 1)
	{
		Fields.erase("source");
	}
	return Node;
}

void CheckEnvironmentLightMigration()
{
	for (const std::uint32_t Version : {1u, 2u})
	{
		const auto Light = ReadValue<FSceneEnvironmentLight>(LegacyEnvironmentLight(Version));
		HYP_CHECK(Light.Source ==
		          (Version == 1 ? ESceneEnvironmentSource::ConstantColor : ESceneEnvironmentSource::SkyAsset));
		HYP_CHECK(std::abs(Light.YawDegrees - 90) < .001f);
		HYP_CHECK(Light.Sky.Id == DefaultSkyReference().Id && Light.Sky.Path == DefaultSkyReference().Path);
		HYP_CHECK(Light.Tint.X == 1 && Light.Tint.Y == 1 && Light.Tint.Z == 1);
		HYP_CHECK(ReadValue<FSceneEnvironmentLight>(WriteValue(Light)) == Light);
	}
	auto PinnedArchive = LegacyEnvironmentLight(2);
	auto& PinnedFields =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(PinnedArchive.Value).at("fields").Value);
	auto Pinned = DefaultSkyReference();
	Pinned.Path = "../Skies/Requested.hasset";
	Pinned.Revision = std::string(64, 'a');
	PinnedFields["sky"] = WriteValue(Pinned);
	HYP_CHECK(ReadValue<FSceneEnvironmentLight>(PinnedArchive).Sky == Pinned);
	FSceneEnvironmentLight Hidden;
	Hidden.Source = ESceneEnvironmentSource::ConstantColor;
	Hidden.Tint = {.25f, .5f, 0};
	Hidden.YawDegrees = -45;
	Hidden.bVisible = false;
	// Values hidden by the selected source remain authored and persist.
	HYP_CHECK(ReadValue<FSceneEnvironmentLight>(WriteValue(Hidden)) == Hidden);
	for (const float Radians : {1e8f, -1e20f, 1e38f, -std::numeric_limits<float>::max()})
	{
		auto Archive = LegacyEnvironmentLight(2);
		auto& Fields =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Archive.Value).at("fields").Value);
		Fields["yawRadians"] = WriteValue(Radians);
		const auto Migrated = ReadValue<FSceneEnvironmentLight>(Archive);
		const double Restored = double(Migrated.YawDegrees) * std::numbers::pi / 180;
		HYP_CHECK(std::isfinite(Migrated.YawDegrees));
		HYP_CHECK(std::abs(std::sin(Restored) - std::sin(double(Radians))) < .00001);
		HYP_CHECK(std::abs(std::cos(Restored) - std::cos(double(Radians))) < .00001);
	}
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
	CheckEnvironmentLightMigration();
}
