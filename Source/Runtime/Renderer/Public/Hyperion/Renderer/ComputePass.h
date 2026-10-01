#pragma once
#include "Hyperion/Materials/MaterialBlocks.h"
#include "Hyperion/Materials/MaterialParameters.h"
#include "Hyperion/Renderer/RenderPass.h"
#include <type_traits>

namespace Hyperion
{
struct FComputeShader
{
	std::filesystem::path Source;
	std::string Entry = "CSMain";
	FShaderCompileOptions Options;
	FShaderParameterContracts Contracts;
	bool operator==(const FComputeShader&) const = default;
};

struct FComputeTextureParameter
{
	FComputeTextureParameter() = default;

	FComputeTextureParameter(std::string InName, std::shared_ptr<const FMaterialTextureSource> InSource,
	                         std::uint32_t InFirstMip = 0, std::uint32_t InMipCount = 1,
	                         EResourceState InAccess = EResourceState::ShaderRead, bool bInFullOverwrite = false,
	                         bool bInInitialized = false, std::uint32_t InArrayIndex = 0)
	    : Name(std::move(InName)), Source(std::move(InSource)), FirstMip(InFirstMip), MipCount(InMipCount),
	      Access(InAccess), bFullOverwrite(bInFullOverwrite), bInitialized(bInInitialized), ArrayIndex(InArrayIndex)
	{
	}

	template<typename T>
	    requires(std::is_enum_v<T> || std::is_same_v<T, FMaterialSemanticId>)
	FComputeTextureParameter(T InSemantic, std::shared_ptr<const FMaterialTextureSource> InSource,
	                         std::uint32_t InFirstMip = 0, std::uint32_t InMipCount = 1,
	                         EResourceState InAccess = EResourceState::ShaderRead, bool bInFullOverwrite = false,
	                         bool bInInitialized = false, std::uint32_t InArrayIndex = 0)
	    : Source(std::move(InSource)), FirstMip(InFirstMip), MipCount(InMipCount), Access(InAccess),
	      bFullOverwrite(bInFullOverwrite), bInitialized(bInInitialized), ArrayIndex(InArrayIndex), Semantic(InSemantic)
	{
	}

	std::string Name;
	std::shared_ptr<const FMaterialTextureSource> Source;
	std::uint32_t FirstMip{};
	std::uint32_t MipCount = 1;
	EResourceState Access = EResourceState::ShaderRead;
	bool bFullOverwrite{};
	bool bInitialized{}; // Explicit promise for a product initialized before this graph; uploads are already defined.
	std::uint32_t ArrayIndex{};
	FMaterialSemanticId Semantic;
};

struct FComputeBufferParameter
{
	FComputeBufferParameter() = default;

	FComputeBufferParameter(std::string InName, FMaterialBufferView InSource,
	                        EResourceState InAccess = EResourceState::ShaderRead, bool bInFullOverwrite = false,
	                        bool bInInitialized = false, std::uint32_t InArrayIndex = 0)
	    : Name(std::move(InName)), View(std::move(InSource)), Access(InAccess), bFullOverwrite(bInFullOverwrite),
	      bInitialized(bInInitialized), ArrayIndex(InArrayIndex)
	{
	}

	template<typename T>
	    requires(std::is_enum_v<T> || std::is_same_v<T, FMaterialSemanticId>)
	FComputeBufferParameter(T InSemantic, FMaterialBufferView InSource,
	                        EResourceState InAccess = EResourceState::ShaderRead, bool bInFullOverwrite = false,
	                        bool bInInitialized = false, std::uint32_t InArrayIndex = 0)
	    : View(std::move(InSource)), Access(InAccess), bFullOverwrite(bInFullOverwrite), bInitialized(bInInitialized),
	      ArrayIndex(InArrayIndex), Semantic(InSemantic)
	{
	}

	std::string Name;
	FMaterialBufferView View;
	EResourceState Access = EResourceState::ShaderRead;
	bool bFullOverwrite{};
	bool bInitialized{};
	std::uint32_t ArrayIndex{};
	FMaterialSemanticId Semantic;
};

struct FComputePassStats
{
	std::uint32_t Dispatches{};
	std::array<std::uint32_t, 3> Groups{};
};

using FComputeSemanticParameter = FMaterialParameterEntry;

// Owned declaration: copy/move numeric values and immutable resource sources before publishing the graph.
struct FComputePassDesc
{
	std::string Name;
	FComputeShader Shader;
	// Numeric names are "ConstantBuffer.Member"; sampler names are their reflected shader identifiers.
	std::vector<std::pair<std::string, FMaterialValue>> Parameters;
	// Engine inputs use generated semantic IDs; explicit custom names above remain supported.
	FMaterialParameterValues EngineParameters;
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
