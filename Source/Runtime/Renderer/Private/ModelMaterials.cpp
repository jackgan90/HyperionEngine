#include "ModelMaterials.h"
#include "Hyperion/Renderer/CascadedShadowMap.h"

namespace Hyperion
{
namespace
{
struct FMaterialAssetLifetime
{
	std::shared_ptr<const FMaterialAssetData> Source;
	std::shared_ptr<const FCompiledMaterialDefinition> Compiled;
};

void SemanticDefaults(FMaterialDescription& InDescription)
{
	const auto Defaults = DefaultShadowParameters();
	for (auto& Parameter : InDescription.Parameters)
	{
		if (Parameter.Source != EMaterialParameterSource::Semantic || Parameter.Default)
		{
			continue;
		}
		for (const auto& Entry : Defaults)
		{
			if (Parameter.Semantic == Entry.Name)
			{
				Parameter.Default = Entry.Value;
				break;
			}
		}
	}
}
} // namespace

std::vector<FVertexAttribute> ModelVertexAttributes()
{
	return {{"POSITION", 0, EVertexFormat::Float3, offsetof(FModelVertex, Position)},
	        {"NORMAL", 0, EVertexFormat::Float3, offsetof(FModelVertex, Normal)},
	        {"TANGENT", 0, EVertexFormat::Float4, offsetof(FModelVertex, Tangent)},
	        {"COLOR", 0, EVertexFormat::Float4, offsetof(FModelVertex, Color)},
	        {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FModelVertex, Uv0)},
	        {"TEXCOORD", 1, EVertexFormat::Float2, offsetof(FModelVertex, Uv1)}};
}

void FMaterialAssetCache::Trim()
{
	std::erase_if(Programs,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Compiled.expired();
	              });
	std::erase_if(Materials,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Surface.expired() && InEntry.second.Declared.expired();
	              });
	std::erase_if(Textures,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Source.expired();
	              });
	const auto Bound = [](auto& InEntries, std::size_t InMaximum)
	{
		while (InEntries.size() >= InMaximum)
		{
			auto Oldest = std::min_element(InEntries.begin(), InEntries.end(),
			                               [](const auto& InA, const auto& InB)
			                               {
				                               return InA.second.Access < InB.second.Access;
			                               });
			InEntries.erase(Oldest);
		}
	};
	Bound(Programs, 1024);
	Bound(Materials, 1024);
	Bound(Textures, 4096);
}

std::shared_ptr<const FMaterialTextureSource> FMaterialAssetCache::Texture(
    const std::shared_ptr<const FTextureAsset>& InAsset)
{
	if (!InAsset)
	{
		throw std::invalid_argument("Null resolved texture asset");
	}
	const auto It = Textures.find(InAsset.get());
	if (It != Textures.end() && It->second.Asset.lock() == InAsset)
	{
		if (auto Existing = It->second.Source.lock())
		{
			++Stats.TextureCacheHits;
			It->second.Access = ++Clock;
			return Existing;
		}
	}
	auto Source = std::make_shared<const FMaterialTextureSource>(InAsset);
	if (Textures.size() >= 4096)
	{
		const auto Oldest = std::min_element(Textures.begin(), Textures.end(),
		                                     [](const auto& InA, const auto& InB)
		                                     {
			                                     return InA.second.Access < InB.second.Access;
		                                     });
		Textures.erase(Oldest);
	}
	Textures[InAsset.get()] = {InAsset, Source, ++Clock};
	++Stats.TextureSources;
	return Source;
}

FRenderMaterialDesc FMaterialAssetCache::Prepare(const std::shared_ptr<const FMaterialAssetData>& InData,
                                                 FShaderCompiler& InCompiler, EShaderFormat InFormat, bool bInCompile)
{
	if (!InData || !InData->Asset)
	{
		throw std::invalid_argument("Material preparation requires resolved native data");
	}
	std::lock_guard Lock(Mutex);
	++Stats.Requests;
	Trim();
	std::vector<const FTextureAsset*> Identity;
	for (const auto& [Reference, Asset] : InData->Textures)
	{
		Identity.push_back(Asset.get());
	}
	const FKey Key{InData->Asset.get(), std::move(Identity), InFormat};
	std::shared_ptr<const FMaterialSnapshot> Existing;
	std::shared_ptr<const FMaterialSnapshot> Declared;
	std::shared_ptr<const FCompiledMaterialDefinition> Compiled;
	if (auto It = Materials.find(Key); It != Materials.end())
	{
		Declared = It->second.Declared.lock();
		Existing = It->second.Surface.lock();
		if (!Existing)
		{
			Existing = Declared;
		}
		Compiled = It->second.Compiled.lock();
		if (Existing && (!bInCompile || (Compiled && Existing->Schema == Compiled->Interface.Schema)))
		{
			It->second.Access = ++Clock;
			++Stats.MaterialCacheHits;
			return {Existing, std::move(Compiled)};
		}
	}
	const auto Resolve = [&](const FAssetRef& InReference)
	{
		const auto It = InData->Textures.find(InReference);
		if (It == InData->Textures.end())
		{
			throw std::runtime_error("Missing resolved material texture: " + InReference.Path);
		}
		return Texture(It->second);
	};
	if (!Existing)
	{
		auto Description = ResolveMaterialAssetDescription(*InData->Asset, Resolve);
		SemanticDefaults(Description);
		FMaterialInstance Instance(std::make_shared<const FMaterialDefinition>(std::move(Description)));
		for (auto& Entry : ResolveMaterialAssetValues(InData->Asset->Values, Resolve))
		{
			Instance.Set(Entry.Name, std::move(Entry.Value));
		}
		Existing = Instance.Freeze();
		++Stats.MaterialPreparations;
	}
	auto Surface = std::make_shared<FMaterialSnapshot>(*Existing);
	if (bInCompile)
	{
		if (!Compiled)
		{
			Compiled = std::make_shared<const FCompiledMaterialDefinition>(
			    CompileMaterialDefinition(InCompiler, Surface->Definition, InFormat));
		}
		Surface->Schema = Compiled->Interface.Schema;
	}
	Surface->Lifetime = std::make_shared<const FMaterialAssetLifetime>(InData, Compiled);
	if (!bInCompile)
	{
		Declared = Surface;
	}
	Materials[Key] = {Surface, Compiled, ++Clock, Declared};
	if (Compiled)
	{
		Programs[Surface->Definition.get()] = {Surface, Compiled, ++Clock};
	}
	return {std::move(Surface), std::move(Compiled)};
}

FMaterialAssetStats FMaterialAssetCache::Statistics() const
{
	std::lock_guard Lock(Mutex);
	auto Result = Stats;
	Result.MaterialEntries = Materials.size();
	Result.TextureEntries = Textures.size();
	return Result;
}

std::shared_ptr<const FCompiledMaterialDefinition> FMaterialAssetCache::FindCompiled(
    const FMaterialDefinition* InDefinition) const
{
	std::lock_guard Lock(Mutex);
	const auto It = Programs.find(InDefinition);
	return It == Programs.end() ? nullptr : It->second.Compiled.lock();
}

FMaterialParameterValues FMaterialAssetCache::ResolveValues(
    const FMaterialAssetValues& InValues, const std::map<FAssetRef, std::shared_ptr<const FTextureAsset>>& InTextures)
{
	std::lock_guard Lock(Mutex);
	Trim();
	return ResolveMaterialAssetValues(InValues,
	                                  [&](const FAssetRef& InReference)
	                                  {
		                                  const auto It = InTextures.find(InReference);
		                                  if (It == InTextures.end())
		                                  {
			                                  throw std::runtime_error("Unresolved local texture asset override");
		                                  }
		                                  return Texture(It->second);
	                                  });
}
} // namespace Hyperion
