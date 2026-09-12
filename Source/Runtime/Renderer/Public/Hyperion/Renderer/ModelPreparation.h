#pragma once
#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
struct FModelVertex
{
	FVec3 Position;
	FVec3 Normal;
	FVec4 Tangent;
	FVec4 Color;
	FVec2 Uv0;
	FVec2 Uv1;
};

struct FPreparedPrimitive
{
	std::vector<FModelVertex> Vertices;
	std::vector<std::uint32_t> Indices;
	FVec3 Center;
};

struct FPreparedModel
{
	std::shared_ptr<const FModelAsset> Source;
	std::vector<FPreparedPrimitive> Primitives;
	FBounds Bounds;
};

// Immutable CPU preparation, normally called on Worker.
FPreparedModel PrepareModel(std::shared_ptr<const FModelAsset> InModel);

} // namespace Hyperion
