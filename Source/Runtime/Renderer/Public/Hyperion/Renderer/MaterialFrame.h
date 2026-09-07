#pragma once
#include "Hyperion/Renderer/MaterialProviders.h"

namespace Hyperion
{
// Main publishes owned inputs once; every view in a family reads this same immutable frame.
struct FMaterialFrameContext
{
	std::uint64_t Session{};
	std::uint64_t Frame{};
	FMaterialProviderInputs Inputs;
};

struct FRenderDrawResult
{
	std::uint64_t Frame{};
	std::uint64_t Family{};
	std::uint64_t View{};
	std::uint64_t Revision{};
	std::uint64_t MaterialRevision{};
	std::string Usage;
	bool bReady{};
	std::string Error;
};
} // namespace Hyperion
