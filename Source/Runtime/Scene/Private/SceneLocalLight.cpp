#include "SceneInternal.h"
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
bool FScenePointLight::operator==(const FScenePointLight& InOther) const
{
	return Color.X == InOther.Color.X && Color.Y == InOther.Color.Y && Color.Z == InOther.Color.Z &&
	       Intensity == InOther.Intensity && Range == InOther.Range;
}

void ValidateScenePointLight(const FScenePointLight& InLight)
{
	SceneLightRadiance(InLight.Color, InLight.Intensity);
	if (!std::isfinite(InLight.Range) || InLight.Range <= 0 || !std::isfinite(1.f / InLight.Range) ||
	    !std::isfinite(InLight.Range * InLight.Range))
	{
		throw std::invalid_argument("Local light requires a positive finite representable range");
	}
}

template<> const FRecordDescriptor& RecordType<FScenePointLight>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FScenePointLight>(
		    "hyperion.scenepointlight",
		    {Member("color", &FScenePointLight::Color, Inspect("Color", 0, {})),
		     Member("intensity", &FScenePointLight::Intensity, Inspect("Intensity", 0, {})),
		     Member("range", &FScenePointLight::Range, Inspect("Range", .000001, {}))},
		    1, ValidateScenePointLight);
		Result.bRejectUnknownFields = true;
		return Result;
	}();
	return Type;
}

const FScenePointLight* FScene::FindPointLight(FSceneHandle InHandle) const
{
	const auto* Node = FindNode(InHandle);
	return Node && Node->PointLight() ? &*Node->PointLight() : nullptr;
}

bool FScene::SetPointLight(FSceneHandle InHandle, FScenePointLight InLight)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (!Existing->PointLight())
	{
		throw std::invalid_argument("Scene node has the wrong payload kind");
	}
	auto Node = *Existing;
	Node.PointLight() = std::move(InLight);
	return Storage->EditNode(InHandle, std::move(Node));
}

FSceneNode MakeScenePointLightNode(std::string InId)
{
	FSceneNode Node;
	Node.Id = std::move(InId);
	Node.PointLight() = FScenePointLight{};
	return Node;
}

bool FSceneSpotLight::operator==(const FSceneSpotLight& InOther) const
{
	return Color.X == InOther.Color.X && Color.Y == InOther.Color.Y && Color.Z == InOther.Color.Z &&
	       Intensity == InOther.Intensity && Range == InOther.Range && InnerRadians == InOther.InnerRadians &&
	       OuterRadians == InOther.OuterRadians;
}

void ValidateSceneSpotLight(const FSceneSpotLight& InLight)
{
	SceneLightRadiance(InLight.Color, InLight.Intensity);
	if (!std::isfinite(InLight.Range) || InLight.Range <= 0 || !std::isfinite(1.f / InLight.Range) ||
	    !std::isfinite(InLight.Range * InLight.Range))
	{
		throw std::invalid_argument("Local light requires a positive finite representable range");
	}
	if (!std::isfinite(InLight.InnerRadians) || !std::isfinite(InLight.OuterRadians) || InLight.InnerRadians < 0 ||
	    InLight.OuterRadians >= 1.5707f || InLight.InnerRadians >= InLight.OuterRadians ||
	    std::cos(InLight.InnerRadians) <= std::cos(InLight.OuterRadians) ||
	    !std::isfinite(InLight.Range * std::tan(InLight.OuterRadians)))
	{
		throw std::invalid_argument("Spot light requires ordered finite half angles below pi/2");
	}
}

template<> const FRecordDescriptor& RecordType<FSceneSpotLight>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneSpotLight>(
		    "hyperion.scenespotlight",
		    {Member("color", &FSceneSpotLight::Color, Inspect("Color", 0, {})),
		     Member("intensity", &FSceneSpotLight::Intensity, Inspect("Intensity", 0, {})),
		     Member("range", &FSceneSpotLight::Range, Inspect("Range", .000001, {})),
		     Member("innerRadians", &FSceneSpotLight::InnerRadians, Inspect("Inner cone (radians)", 0, {})),
		     Member("outerRadians", &FSceneSpotLight::OuterRadians, Inspect("Outer cone (radians)", .000001, {}))},
		    1, ValidateSceneSpotLight);
		Result.bRejectUnknownFields = true;
		return Result;
	}();
	return Type;
}

const FSceneSpotLight* FScene::FindSpotLight(FSceneHandle InHandle) const
{
	const auto* Node = FindNode(InHandle);
	return Node && Node->SpotLight() ? &*Node->SpotLight() : nullptr;
}

bool FScene::SetSpotLight(FSceneHandle InHandle, FSceneSpotLight InLight)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (!Existing->SpotLight())
	{
		throw std::invalid_argument("Scene node has the wrong payload kind");
	}
	auto Node = *Existing;
	Node.SpotLight() = std::move(InLight);
	return Storage->EditNode(InHandle, std::move(Node));
}

FSceneNode MakeSceneSpotLightNode(std::string InId)
{
	FSceneNode Node;
	Node.Id = std::move(InId);
	Node.SpotLight() = FSceneSpotLight{};
	return Node;
}

} // namespace Hyperion
