#pragma once
#include "Hyperion/RHI/RHIDevice.h"
#include "Hyperion/Renderer/FullscreenPass.h"
#include "Hyperion/Renderer/MaterialPreparation.h"
#include <map>

namespace Hyperion
{
struct FFullscreenResources
{
	FDrawPacket Triangle;

	struct FMaterialEntry
	{
		std::shared_ptr<const FCompiledMaterialDefinition> Program;
		std::unique_ptr<FMaterialInstance> Instance;
		FMaterialParameterValues Values;
		std::weak_ptr<const void> Lifetime;
	};

	std::map<std::uint64_t, FMaterialEntry> Materials;
	void Initialize(IRHIDevice& InDevice);
	void Collect();
};
} // namespace Hyperion
