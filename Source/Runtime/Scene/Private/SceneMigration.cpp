#include "Hyperion/Scene/SceneManifest.h"
#include <set>

namespace Hyperion
{
namespace
{
std::string MigrationId(std::string InBase, std::set<std::string>& InUsed)
{
	auto Candidate = InBase;
	std::uint64_t Suffix{};
	while (!InUsed.insert(Candidate).second)
	{
		Candidate = InBase + "-" + std::to_string(++Suffix);
	}
	return Candidate;
}

template<class T>
T LegacyField(const FArchiveNode::FObject& InFields, const char* InName, T InDefault,
              const FRecordReadContext& InContext)
{
	const auto Found = InFields.find(InName);
	return Found == InFields.end() ? InDefault : ReadValue<T>(Found->second, InContext.Child(InName));
}
} // namespace

FSceneManifest UpgradeLegacyScene(const FLegacySceneManifest& InManifest)
{
	ValidateLegacySceneManifest(InManifest);
	FSceneManifest Result;
	Result.Assets = InManifest.Assets;
	std::set<std::string> Used;
	Result.Nodes.reserve(InManifest.Instances.size() + 3);
	for (const auto& Instance : InManifest.Instances)
	{
		Used.insert(Instance.Id);
		FSceneNodeEntry Node;
		Node.Id = Instance.Id;
		Node.Name = Instance.Name.empty() ? Instance.Id : Instance.Name;
		Node.Transform = Instance.Transform;
		Node.Model = FSceneNodeModel{Instance.Asset, Instance.bVisible, Instance.Material, Instance.Surface,
		                             Instance.SectionSurfaces};
		Result.Nodes.push_back(std::move(Node));
	}
	Result.DefaultCamera = MigrationId("camera-main", Used);
	Result.MainDirectionalLight = MigrationId("light-main", Used);
	Result.EnvironmentLight = MigrationId("light-environment", Used);
	FSceneCamera Lens;
	Lens.Near = InManifest.Near;
	Lens.Far = InManifest.Far;
	Result.Nodes.push_back(
	    SceneEntryFromNode(MakeSceneCameraNode(Result.DefaultCamera, InManifest.Eye, InManifest.Target, Lens)));
	Result.Nodes.push_back(SceneEntryFromNode(MakeSceneDirectionalLightNode(Result.MainDirectionalLight)));
	Result.Nodes.push_back(SceneEntryFromNode(MakeSceneEnvironmentLightNode(Result.EnvironmentLight)));
	ValidateSceneManifest(Result);
	return Result;
}

void MigrateSceneNodes(FArchiveNode::FObject& InFields, const FRecordReadContext& InContext)
{
	for (const auto* Name : {"nodes", "defaultCamera", "mainDirectionalLight", "environmentLight"})
	{
		if (InFields.contains(Name))
		{
			throw std::invalid_argument("Conflicting legacy and node scene fields");
		}
	}
	FLegacySceneManifest Legacy;
	Legacy.Assets = ReadValue<std::vector<FSceneAssetEntry>>(InFields.at("assets"), InContext.Child("assets"));
	// ReadValue executes every nested asset/instance/material migration before conversion.
	Legacy.Instances =
	    ReadValue<std::vector<FSceneInstanceEntry>>(InFields.at("instances"), InContext.Child("instances"));
	Legacy.Eye = LegacyField(InFields, "eye", Legacy.Eye, InContext);
	Legacy.Target = LegacyField(InFields, "target", Legacy.Target, InContext);
	Legacy.Near = LegacyField(InFields, "near", Legacy.Near, InContext);
	Legacy.Far = LegacyField(InFields, "far", Legacy.Far, InContext);
	auto Archive = WriteValue(UpgradeLegacyScene(Legacy));
	auto& Object = std::get<FArchiveNode::FObject>(Archive.Value);
	InFields = std::move(std::get<FArchiveNode::FObject>(Object.at("fields").Value));
}
} // namespace Hyperion
