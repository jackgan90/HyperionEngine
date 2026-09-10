#pragma once
#include "Hyperion/Renderer/MaterialBindingContext.h"

namespace Hyperion
{
// Immutable proof built after full dependency/instance validation. No object scopes or resource leases are owned.
struct FMaterialSharedBinding
{
	struct FResource
	{
		std::size_t Index{};
		std::weak_ptr<const FMaterialValue> Value;
	};

	std::shared_ptr<const FCompiledMaterialDefinition> Program;
	const FCompiledMaterialPass* Pass{};
	std::vector<std::size_t> Parameters;
	std::vector<std::uint32_t> Dependencies;
	std::vector<FResource> Resources;
};
} // namespace Hyperion
