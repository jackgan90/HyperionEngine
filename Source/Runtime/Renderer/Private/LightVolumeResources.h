#pragma once
#include "FullscreenPass.h"

namespace Hyperion
{
struct FLightVolumeResources
{
	FDrawPacket Sphere;
	FDrawPacket Cone;
	FFullscreenResources::FMaterialEntry Material;
	void Initialize(IRHIDevice& InDevice);
};
} // namespace Hyperion
