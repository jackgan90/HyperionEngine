#include "Hyperion/Scene/SceneManifest.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include <set>

namespace Hyperion
{
void ValidateSceneManifest(const FSceneManifest& InManifest)
{
	std::set<std::string> Assets;
	std::set<std::string> Instances;
	for (const auto& Asset : InManifest.Assets)
	{
		ValidateAssetRef(Asset.Reference);
		if (Asset.Id.empty() || !Assets.insert(Asset.Id).second ||
		    Asset.Reference.TypeId != RecordType<FModelAsset>().Id)
		{
			throw std::invalid_argument("Scene asset IDs must be unique and references must target models");
		}
	}
	for (const auto& Instance : InManifest.Instances)
	{
		const auto& Values = Instance.Transform.Values;
		if (Instance.Id.empty() || !Instances.insert(Instance.Id).second || !Assets.contains(Instance.Asset) ||
		    !std::all_of(Values.begin(), Values.end(),
		                 [](float InValue)
		                 {
			                 return std::isfinite(InValue);
		                 }) ||
		    Values[3] != 0 || Values[7] != 0 || Values[11] != 0 || Values[15] != 1)
		{
			throw std::invalid_argument("Invalid scene instance ID, asset reference or affine transform");
		}
		ValidateMaterialOverride(Instance.Material);
		ValidateSceneMaterialAsset(Instance.Surface);
		std::set<std::uint32_t> Sections;
		for (const auto& Section : Instance.SectionSurfaces)
		{
			if (!Sections.insert(Section.Section).second)
			{
				throw std::invalid_argument("Duplicate persistent scene material section");
			}
			ValidateSceneMaterialAsset(Section.Material);
		}
	}
	const auto Direction = Subtract(InManifest.Target, InManifest.Eye);
	if (!IsFinite(InManifest.Eye) || !IsFinite(InManifest.Target) || !IsFinite(Direction) ||
	    !std::isfinite(Length(Direction)) || Length(Direction) < .001f || Length(Cross(Direction, {0, 1, 0})) < .001f ||
	    !std::isfinite(InManifest.Near) || !std::isfinite(InManifest.Far) || InManifest.Near <= 0 ||
	    InManifest.Far <= InManifest.Near)
	{
		throw std::invalid_argument("Invalid scene camera");
	}
}

template<> const FRecordDescriptor& RecordType<FSceneAssetEntry>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneAssetEntry>(
		    "hyperion.sceneasset",
		    {Member("id", &FSceneAssetEntry::Id, {true}), Member("reference", &FSceneAssetEntry::Reference, {true})},
		    2);
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject& InFields)
		                          {
			                          const auto Path = ReadValue<std::string>(InFields.at("path"));
			                          if (InFields.contains("reference"))
			                          {
				                          throw std::runtime_error("Conflicting legacy scene reference");
			                          }
			                          InFields["reference"] =
			                              WriteValue(FAssetRef{{}, Path, RecordType<FModelAsset>().Id, {}});
			                          InFields.erase("path");
		                          });
		return Result;
	}();
	return Type;
}

namespace
{
void MigrateInstance(FArchiveNode::FObject& InFields)
{
	const auto Vector = [&](const char* InName, FVec3 InDefault)
	{
		const auto It = InFields.find(InName);
		return It == InFields.end() ? InDefault : ReadValue<FVec3>(It->second);
	};
	const auto Rotation = InFields.find("rotation");
	const auto Q = Rotation == InFields.end() ? FVec4{0, 0, 0, 1} : ReadValue<FVec4>(Rotation->second);
	const double Norm = double(Q.X) * Q.X + double(Q.Y) * Q.Y + double(Q.Z) * Q.Z + double(Q.W) * Q.W;
	if (!std::isfinite(Norm) || std::abs(Norm - 1) > .001 || InFields.contains("transform"))
	{
		throw std::runtime_error("Invalid legacy scene rotation or conflicting transform");
	}
	InFields["transform"] = WriteValue(ComposeTRS(Vector("translation", {}), Q, Vector("scale", {1, 1, 1})));
	InFields.erase("translation");
	InFields.erase("rotation");
	InFields.erase("scale");
}
} // namespace

template<> const FRecordDescriptor& RecordType<FSceneInstanceEntry>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneInstanceEntry>(
		    "hyperion.sceneinstance",
		    {Member("id", &FSceneInstanceEntry::Id, {true}), Member("asset", &FSceneInstanceEntry::Asset, {true}),
		     Member("transform", &FSceneInstanceEntry::Transform), Member("visible", &FSceneInstanceEntry::bVisible),
		     Member("name", &FSceneInstanceEntry::Name), Member("material", &FSceneInstanceEntry::Material),
		     Member("surface", &FSceneInstanceEntry::Surface),
		     Member("sectionSurfaces", &FSceneInstanceEntry::SectionSurfaces)},
		    3);
		Result.Migrations.emplace(1, MigrateInstance);
		Result.Migrations.emplace(2,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		return Result;
	}();
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneManifest>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneManifest>(
		    "hyperion.scene",
		    {Member("assets", &FSceneManifest::Assets, {true}), Member("instances", &FSceneManifest::Instances, {true}),
		     Member("eye", &FSceneManifest::Eye), Member("target", &FSceneManifest::Target),
		     Member("near", &FSceneManifest::Near), Member("far", &FSceneManifest::Far)},
		    3, ValidateSceneManifest);
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		Result.Migrations.emplace(2,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		return Result;
	}();
	return Type;
}

void RegisterSceneAssetTypes(FRecordRegistry& InRegistry)
{
	InRegistry.Register<FModelAsset>();
	InRegistry.Register<FMaterialAsset>();
	InRegistry.Register<FTextureAsset>();
	InRegistry.Register<FSceneManifest>();
}
} // namespace Hyperion
