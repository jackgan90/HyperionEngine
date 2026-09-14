#pragma once
#include "Hyperion/Materials/MaterialParameters.h"
#include "Hyperion/Renderer/RenderPass.h"

namespace Hyperion
{
struct FComputeShader
{
	std::filesystem::path Source;
	std::string Entry = "CSMain";
	FShaderCompileOptions Options;
	bool operator==(const FComputeShader&) const = default;
};

struct FComputeTextureParameter
{
	std::string Name;
	std::shared_ptr<const FMaterialTextureSource> Source;
	std::uint32_t FirstMip{};
	std::uint32_t MipCount = 1;
	EResourceState Access = EResourceState::ShaderRead;
	bool bFullOverwrite{};
	bool bInitialized{}; // Explicit promise for a product initialized before this graph; uploads are already defined.
	std::uint32_t ArrayIndex{};
};

struct FComputeBufferParameter
{
	std::string Name;
	FMaterialBufferView View;
	EResourceState Access = EResourceState::ShaderRead;
	bool bFullOverwrite{};
	bool bInitialized{};
	std::uint32_t ArrayIndex{};
};

struct FComputePassStats
{
	std::uint32_t Dispatches{};
	std::array<std::uint32_t, 3> Groups{};
};

// Owned declaration: copy/move numeric values and immutable resource sources before publishing the graph.
struct FComputePassDesc
{
	std::string Name;
	FComputeShader Shader;
	// Numeric names are "ConstantBuffer.Member"; sampler names are their reflected shader identifiers.
	std::vector<std::pair<std::string, FMaterialValue>> Parameters;
	std::vector<FComputeTextureParameter> Textures;
	std::vector<FComputeBufferParameter> Buffers;
	std::array<std::uint32_t, 3> Extent{1, 1,
	                                    1}; // Rounded up by reflected numthreads; shader bounds checks are required.
	std::shared_ptr<const void> Lifetime;
	std::vector<std::size_t> After;
	std::shared_ptr<FComputePassStats> Statistics;
};

class FRenderSession;
void AddComputePass(FRenderSession& InSession, FRenderGraph& InGraph, FComputePassDesc InPass,
                    bool bInDeferPreparation = true);
} // namespace Hyperion
