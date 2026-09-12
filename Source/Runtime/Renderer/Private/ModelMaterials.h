#pragma once
#include "Hyperion/Renderer/ModelPreparation.h"
#include "Hyperion/Renderer/RenderResources.h"
#include <mutex>

namespace Hyperion
{
std::vector<FVertexAttribute> ModelVertexAttributes();

class FMaterialAssetCache
{
public:
	FRenderMaterialDesc Prepare(const std::shared_ptr<const FMaterialAssetData>& InData, FShaderCompiler& InCompiler,
	                            EShaderFormat InFormat, bool bInCompile = true);
	FMaterialAssetStats Statistics() const;
	std::shared_ptr<const FCompiledMaterialDefinition> FindCompiled(const FMaterialDefinition* InDefinition) const;
	FMaterialParameterValues ResolveValues(const FMaterialAssetValues& InValues,
	                                       const std::map<FAssetRef, std::shared_ptr<const FTextureAsset>>& InTextures);

private:
	using FKey = std::tuple<const FMaterialAsset*, std::vector<const FTextureAsset*>, EShaderFormat>;

	struct FMaterialEntry
	{
		std::weak_ptr<const FMaterialSnapshot> Surface;
		std::weak_ptr<const FCompiledMaterialDefinition> Compiled;
		std::uint64_t Access{};
		std::weak_ptr<const FMaterialSnapshot> Declared;
	};

	struct FTextureEntry
	{
		std::weak_ptr<const FTextureAsset> Asset;
		std::weak_ptr<const FMaterialTextureSource> Source;
		std::uint64_t Access{};
	};

	std::shared_ptr<const FMaterialTextureSource> Texture(const std::shared_ptr<const FTextureAsset>& InAsset);
	void Trim();
	mutable std::mutex Mutex;
	std::map<FKey, FMaterialEntry> Materials;
	std::map<const FTextureAsset*, FTextureEntry> Textures;
	std::map<const FMaterialDefinition*, FMaterialEntry> Programs;
	FMaterialAssetStats Stats;
	std::uint64_t Clock{};
};
} // namespace Hyperion
