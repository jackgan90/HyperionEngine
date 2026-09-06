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

struct FPreparedMaterial
{
	FModelMaterial Material;
	std::array<std::uint32_t, 5> Textures{};
	std::array<FSamplerDesc, 5> Samplers;
};

struct FPreparedModel
{
	std::shared_ptr<const FModelAsset> Source;
	std::vector<FPreparedPrimitive> Primitives;
	std::vector<FPreparedMaterial> Materials;
	std::vector<FTextureDesc> Textures;
	FBounds Bounds;
};

// CPU work only. All methods of FModelRenderer execute on the RHI coordinator.
FPreparedModel PrepareModel(std::shared_ptr<const FModelAsset> InModel);

class FModelRenderer
{
public:
	FModelRenderer(IRHIDevice& InDevice, const FPreparedModel& InModel, const FShaderArtifact& InVertex,
	               const FShaderArtifact& InPixel);
	bool Ready();
	std::vector<FDrawPacket> Draws(const FMat4& InViewProjection, FVec3 InEye, FSize InSize);

private:
	IRHIDevice& Device;
	std::shared_ptr<const FModelAsset> Source;
	std::vector<FModelInstance> Instances;
	std::vector<FBuffer> Vertices;
	std::vector<FBuffer> Indices;
	std::vector<FVec3> Centers;
	std::vector<FTexture> Textures;
	std::vector<FPreparedMaterial> Materials;
	std::vector<std::array<FPipeline, 2>> Pipelines;
};
} // namespace Hyperion
