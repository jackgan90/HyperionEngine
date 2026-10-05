#pragma once
#include "Hyperion/Platform/Window.h"
#include "Hyperion/Scene/Scene.h"
#include <string>
#include <vector>

namespace Hyperion
{
struct FViewportClick
{
	FVec2 Start;
	bool bToggle{};
	FVec4 Bounds;
	FSize Size;
	FSceneCameraView Camera;
	std::uint64_t Revision{};
};

} // namespace Hyperion
