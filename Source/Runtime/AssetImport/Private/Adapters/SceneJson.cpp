#include "Hyperion/AssetImport/MaterialImport.h"
#include "Hyperion/AssetImport/SceneImport.h"
#include "Hyperion/AssetImport/SkyImport.h"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace Hyperion
{
namespace
{
using FJson = nlohmann::json;

FVec3 Vector3(const FJson& InValue)
{
	if (!InValue.is_array() || InValue.size() != 3)
	{
		throw std::invalid_argument("Expected a scene vector with three numbers");
	}
	return {InValue.at(0).get<float>(), InValue.at(1).get<float>(), InValue.at(2).get<float>()};
}

FVec4 Quaternion(const FJson& InValue)
{
	if (!InValue.is_array() || InValue.size() != 4)
	{
		throw std::invalid_argument("Expected a scene quaternion [x,y,z,w]");
	}
	const FVec4 Result{InValue.at(0).get<float>(), InValue.at(1).get<float>(), InValue.at(2).get<float>(),
	                   InValue.at(3).get<float>()};
	const double Norm = double(Result.X) * Result.X + double(Result.Y) * Result.Y + double(Result.Z) * Result.Z +
	                    double(Result.W) * Result.W;
	if (!std::isfinite(Norm) || std::abs(Norm - 1) > .001)
	{
		throw std::invalid_argument("Invalid scene rotation quaternion");
	}
	return Result;
}

void Fields(const FJson& InObject, std::initializer_list<std::string_view> InAllowed)
{
	if (!InObject.is_object())
	{
		throw std::invalid_argument("Expected a scene object");
	}
	for (const auto& [Key, Value] : InObject.items())
	{
		if (std::find(InAllowed.begin(), InAllowed.end(), Key) == InAllowed.end())
		{
			throw std::invalid_argument("Unsupported scene field: " + Key);
		}
	}
}

FMat4 NodeTransform(const FJson& InNode)
{
	if (InNode.contains("transform"))
	{
		if (InNode.contains("translation") || InNode.contains("rotation") || InNode.contains("scale"))
		{
			throw std::invalid_argument("Scene transform and TRS are mutually exclusive");
		}
		const auto& Array = InNode.at("transform");
		if (!Array.is_array() || Array.size() != 16)
		{
			throw std::invalid_argument("Expected sixteen column-major scene matrix numbers");
		}
		FMat4 Result;
		for (std::size_t Index = 0; Index < 16; ++Index)
		{
			Result.Values[Index] = Array.at(Index).get<float>();
		}
		return Result;
	}
	return ComposeTRS(Vector3(InNode.value("translation", FJson{0, 0, 0})),
	                  Quaternion(InNode.value("rotation", FJson{0, 0, 0, 1})),
	                  Vector3(InNode.value("scale", FJson{1, 1, 1})));
}

FSceneNodeModel ModelPayload(const FJson& InJson)
{
	Fields(InJson, {"asset", "visible", "material", "surface", "sectionSurfaces"});
	FSceneNodeModel Result;
	Result.Asset = InJson.at("asset").get<std::string>();
	Result.bVisible = InJson.value("visible", true);
	if (InJson.contains("material"))
	{
		Result.Material = ReadValue<FMaterialOverride>(DecodeAssetSourceJson(InJson.at("material").dump()));
	}
	if (InJson.contains("surface"))
	{
		Result.Surface = ReadValue<FSceneMaterialAsset>(DecodeAssetSourceJson(InJson.at("surface").dump()));
	}
	if (InJson.contains("sectionSurfaces"))
	{
		Result.SectionSurfaces =
		    ReadValue<std::vector<FSceneSectionMaterial>>(DecodeAssetSourceJson(InJson.at("sectionSurfaces").dump()));
	}
	return Result;
}

FSceneCamera CameraPayload(const FJson& InJson)
{
	Fields(InJson, {"verticalRadians", "near", "far", "focusDistance"});
	FSceneCamera Result;
	Result.VerticalRadians = InJson.value("verticalRadians", Result.VerticalRadians);
	Result.Near = InJson.value("near", Result.Near);
	Result.Far = InJson.value("far", Result.Far);
	Result.FocusDistance = InJson.value("focusDistance", Result.FocusDistance);
	return Result;
}

FSceneNodeEntry NodeEntry(const FJson& InJson)
{
	Fields(InJson, {"id", "name", "parent", "enabled", "transform", "translation", "rotation", "scale", "model",
	                "camera", "directionalLight", "environmentLight", "pointLight", "spotLight"});
	FSceneNodeEntry Result;
	Result.Id = InJson.at("id").get<std::string>();
	Result.Name = InJson.value("name", Result.Id);
	Result.Parent = InJson.value("parent", std::string{});
	Result.bEnabled = InJson.value("enabled", true);
	Result.Transform = NodeTransform(InJson);
	if (InJson.contains("model"))
	{
		Result.Model = ModelPayload(InJson.at("model"));
	}
	if (InJson.contains("camera"))
	{
		Result.Camera = CameraPayload(InJson.at("camera"));
	}
	if (InJson.contains("directionalLight"))
	{
		const auto& Light = InJson.at("directionalLight");
		Fields(Light, {"color", "intensity", "castShadows"});
		Result.DirectionalLight =
		    FSceneDirectionalLight{Vector3(Light.value("color", FJson{1, 1, 1})), Light.value("intensity", 1.f),
		                           Light.value("castShadows", true)};
	}
	if (InJson.contains("environmentLight"))
	{
		const auto& Light = InJson.at("environmentLight");
		Fields(Light, {"color", "intensity", "source", "sky", "yawRadians", "visible"});
		Result.EnvironmentLight =
		    FSceneEnvironmentLight{Vector3(Light.value("color", FJson{1, 1, 1})), Light.value("intensity", 1.f)};
		auto& Environment = *Result.EnvironmentLight;
		const auto Source = Light.value("source", std::string("ConstantColor"));
		if (Source != "ConstantColor" && Source != "SkyAsset")
		{
			throw std::invalid_argument("Unsupported environment source");
		}
		Environment.Source =
		    Source == "SkyAsset" ? ESceneEnvironmentSource::SkyAsset : ESceneEnvironmentSource::ConstantColor;
		if (Light.contains("sky"))
		{
			Environment.Sky = FAssetRef{"", Light.at("sky").get<std::string>(), RecordType<FSkyAsset>().Id, ""};
		}
		Environment.YawRadians = Light.value("yawRadians", 0.f);
		Environment.bVisible = Light.value("visible", true);
	}
	if (InJson.contains("pointLight"))
	{
		const auto& Light = InJson.at("pointLight");
		Fields(Light, {"color", "intensity", "range"});
		Result.PointLight = FScenePointLight{Vector3(Light.value("color", FJson{1, 1, 1})),
		                                     Light.value("intensity", 10.f), Light.value("range", 5.f)};
	}
	if (InJson.contains("spotLight"))
	{
		const auto& Light = InJson.at("spotLight");
		Fields(Light, {"color", "intensity", "range", "innerRadians", "outerRadians"});
		Result.SpotLight = FSceneSpotLight{Vector3(Light.value("color", FJson{1, 1, 1})),
		                                   Light.value("intensity", 10.f), Light.value("range", 5.f),
		                                   Light.value("innerRadians", .35f), Light.value("outerRadians", .6f)};
	}
	return Result;
}

std::vector<FSceneAssetEntry> SceneAssets(const FJson& InArray)
{
	if (!InArray.is_array())
	{
		throw std::invalid_argument("Expected scene assets array");
	}
	std::vector<FSceneAssetEntry> Result;
	for (const auto& Asset : InArray)
	{
		Fields(Asset, {"id", "path"});
		Result.push_back({Asset.at("id").get<std::string>(),
		                  {{}, Asset.at("path").get<std::string>(), RecordType<FModelAsset>().Id, {}}});
	}
	return Result;
}

FSceneManifest DecodeLegacy(const FJson& InJson)
{
	Fields(InJson, {"type", "schema_version", "assets", "instances", "camera"});
	if (!InJson.at("instances").is_array())
	{
		throw std::invalid_argument("Expected legacy scene instances array");
	}
	FLegacySceneManifest Manifest;
	Manifest.Assets = SceneAssets(InJson.at("assets"));
	for (const auto& Instance : InJson.at("instances"))
	{
		Fields(Instance, {"id", "name", "asset", "visible", "translation", "rotation", "scale"});
		FSceneInstanceEntry Entry;
		Entry.Id = Instance.at("id").get<std::string>();
		Entry.Name = Instance.value("name", Entry.Id);
		Entry.Asset = Instance.at("asset").get<std::string>();
		Entry.Transform = NodeTransform(Instance);
		Entry.bVisible = Instance.value("visible", true);
		Manifest.Instances.push_back(std::move(Entry));
	}
	if (InJson.contains("camera"))
	{
		const auto& Camera = InJson.at("camera");
		Fields(Camera, {"eye", "target", "near", "far"});
		Manifest.Eye = Vector3(Camera.at("eye"));
		Manifest.Target = Vector3(Camera.at("target"));
		Manifest.Near = Camera.value("near", .01f);
		Manifest.Far = Camera.value("far", 1000.f);
	}
	return UpgradeLegacyScene(Manifest);
}
} // namespace

FSceneManifest DecodeSceneManifest(std::string_view InText)
{
	// The shared archive adapter rejects duplicate keys before nlohmann can overwrite them.
	const auto Archive = DecodeAssetSourceJson(InText);
	const auto Json = FJson::parse(InText);
	if (Json.contains("fields"))
	{
		return ReadValue<FSceneManifest>(Archive);
	}
	if (Json.at("type") != "hyperion.scene")
	{
		throw std::invalid_argument("Unsupported scene manifest type");
	}
	if (Json.at("schema_version") == 1)
	{
		return DecodeLegacy(Json);
	}
	Fields(Json,
	       {"type", "schema_version", "assets", "nodes", "defaultCamera", "mainDirectionalLight", "environmentLight"});
	if ((Json.at("schema_version") != 2 && Json.at("schema_version") != 3) || !Json.at("nodes").is_array())
	{
		throw std::invalid_argument("Unsupported scene manifest version or nodes");
	}
	FSceneManifest Manifest;
	Manifest.Assets = SceneAssets(Json.at("assets"));
	for (const auto& Node : Json.at("nodes"))
	{
		if (Json.at("schema_version") == 2 && (Node.contains("pointLight") || Node.contains("spotLight")))
		{
			throw std::invalid_argument("Local light payloads require scene source version 3");
		}
		Manifest.Nodes.push_back(NodeEntry(Node));
	}
	Manifest.DefaultCamera = Json.value("defaultCamera", std::string{});
	Manifest.MainDirectionalLight = Json.value("mainDirectionalLight", std::string{});
	Manifest.EnvironmentLight = Json.value("environmentLight", std::string{});
	ValidateSceneManifest(Manifest);
	return Manifest;
}

void RegisterSceneImporter(FAssetImportService& InImports)
{
	RegisterSkyImporter(InImports);
	InImports.Register({"hyperion.scene-json",
	                    7,
	                    &RecordType<FSceneManifest>(),
	                    {".json"},
	                    [](FAssetImportContext& InContext)
	                    {
		                    const auto& Bytes = *InContext.Bytes;
		                    return std::make_shared<FSceneManifest>(
		                        DecodeSceneManifest({reinterpret_cast<const char*>(Bytes.data()), Bytes.size()}));
	                    }});
	InImports.Register({"hyperion.native-scene-upgrade",
	                    7,
	                    &RecordType<FSceneManifest>(),
	                    {".hasset"},
	                    [](FAssetImportContext& InContext)
	                    {
		                    return ReadRecord(RecordType<FSceneManifest>(), DecodeAsset(InContext.Bytes).Object);
	                    }});
}
} // namespace Hyperion
