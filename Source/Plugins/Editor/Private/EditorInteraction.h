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

struct FReparentGesture
{
	std::string Token;
	std::string Document;
	std::uint64_t Revision{};
	std::vector<FSceneHandle> Handles;
	FSceneHandle Source;
	FVec2 Start;
	FVec4 Bounds;
	bool bToggle{};
	bool bRange{};
	bool bDragging{};
	bool bTargetPreview{};
};
} // namespace Hyperion
