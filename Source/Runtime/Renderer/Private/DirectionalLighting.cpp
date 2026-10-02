#include "DirectionalLighting.h"
#include "LightingWireContracts.h"
#include <algorithm>

namespace Hyperion
{
FMaterialValue DirectionalLightBuffer(const FSceneMetadata* InMetadata, const FMaterialValue* InPrevious)
{
	const auto& Strides = GetLightingWireStrides();
	std::vector<FDirectionalLightData> Lights;
	if (InMetadata)
	{
		for (const auto& [Handle, Entry] : InMetadata->DirectionalLights)
		{
			if (!Entry.bEnabled || InMetadata->Lighting.Directional.Handle == Handle)
			{
				continue;
			}
			const auto Radiance = SceneLightRadiance(Entry.Light.Color, Entry.Light.Intensity);
			if (Radiance.X > 0 || Radiance.Y > 0 || Radiance.Z > 0)
			{
				Lights.push_back({{Entry.SurfaceToLight.X, Entry.SurfaceToLight.Y, Entry.SurfaceToLight.Z, 0},
				                  {Radiance.X, Radiance.Y, Radiance.Z, 0}});
			}
		}
	}
	if (Lights.empty())
	{
		Lights.push_back({{0, 0, 1, 0}, {}});
	}
	const auto Bytes = std::as_bytes(std::span(Lights));
	if (InPrevious && InPrevious->Buffer.Source && std::ranges::equal(Bytes, InPrevious->Buffer.Source->GetBytes()))
	{
		return FMaterialValue::FromBuffer({InPrevious->Buffer.Source, EMaterialBufferViewKind::Structured, 0,
		                                   Bytes.size(), Strides.DirectionalLight});
	}
	static const auto Empty = std::make_shared<const FMaterialReadBufferSource>(std::as_bytes(
	    std::span<const FDirectionalLightData>(std::array<FDirectionalLightData, 1>{{{{0, 0, 1, 0}, {}}}})));
	const auto Source = !InMetadata ? Empty : std::make_shared<const FMaterialReadBufferSource>(Bytes);
	return FMaterialValue::FromBuffer(
	    {Source, EMaterialBufferViewKind::Structured, 0, Source->GetBytes().size(), Strides.DirectionalLight});
}
} // namespace Hyperion
