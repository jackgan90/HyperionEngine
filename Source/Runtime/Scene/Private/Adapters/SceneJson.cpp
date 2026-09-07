#include "Hyperion/Scene/SceneManifest.h"
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
	return {InValue.at(0).get<float>(), InValue.at(1).get<float>(), InValue.at(2).get<float>(),
	        InValue.at(3).get<float>()};
}
} // namespace

FSceneManifest DecodeSceneManifest(std::string_view InText)
{
	const auto Json = FJson::parse(InText);
	if (Json.at("type") != "hyperion.scene" || Json.at("schema_version") != 1 || !Json.at("assets").is_array() ||
	    !Json.at("instances").is_array())
	{
		throw std::invalid_argument("Unsupported scene manifest type, version or arrays");
	}
	FSceneManifest Manifest;
	for (const auto& Asset : Json.at("assets"))
	{
		Manifest.Assets.push_back({Asset.at("id").get<std::string>(), Asset.at("path").get<std::string>()});
	}
	for (const auto& Instance : Json.at("instances"))
	{
		FSceneInstanceEntry Entry;
		Entry.Id = Instance.at("id").get<std::string>();
		Entry.Asset = Instance.at("asset").get<std::string>();
		Entry.Translation = Vector3(Instance.value("translation", FJson{0, 0, 0}));
		Entry.Rotation = Quaternion(Instance.value("rotation", FJson{0, 0, 0, 1}));
		Entry.Scale = Vector3(Instance.value("scale", FJson{1, 1, 1}));
		Entry.bVisible = Instance.value("visible", true);
		Manifest.Instances.push_back(std::move(Entry));
	}
	if (Json.contains("camera"))
	{
		const auto& Camera = Json.at("camera");
		Manifest.Eye = Vector3(Camera.at("eye"));
		Manifest.Target = Vector3(Camera.at("target"));
		Manifest.Near = Camera.value("near", .01f);
		Manifest.Far = Camera.value("far", 1000.f);
	}
	ValidateSceneManifest(Manifest);
	return Manifest;
}
} // namespace Hyperion
