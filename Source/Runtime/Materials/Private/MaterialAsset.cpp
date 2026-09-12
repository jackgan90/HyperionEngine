#include "Hyperion/Materials/MaterialAsset.h"
#include <set>

namespace Hyperion
{
namespace
{
FMaterialValue ResolveValue(const FMaterialAssetValue& InValue, const FMaterialTextureResolver& InResolve,
                            std::uint32_t InDepth)
{
	if (InDepth > 32 || InValue.Type.Kind == EMaterialValueKind::ReadBuffer ||
	    (InValue.Type.Kind == EMaterialValueKind::Texture2D) != InValue.Texture.has_value())
	{
		throw std::invalid_argument("Invalid persistent material resource or nesting depth");
	}
	FMaterialValue Result;
	Result.Type = InValue.Type;
	Result.Words = InValue.Words;
	Result.Sampler = InValue.Sampler;
	if (InValue.Texture)
	{
		ValidateAssetRef(*InValue.Texture);
		if (InValue.Texture->TypeId != RecordType<FTextureAsset>().Id || !InResolve)
		{
			throw std::invalid_argument("Persistent material texture requires a typed asset resolver");
		}
		Result.Texture = InResolve(*InValue.Texture);
		if (!Result.Texture || Result.Texture->IsRenderTarget())
		{
			throw std::invalid_argument("Persistent material cannot resolve to a runtime render target");
		}
	}
	for (const auto& Element : InValue.Elements)
	{
		Result.Elements.push_back(ResolveValue(Element, InResolve, InDepth + 1));
	}
	Result.Validate();
	return Result;
}

std::shared_ptr<const FMaterialTextureSource> ValidationTexture(const FAssetRef&)
{
	static const auto Source = std::make_shared<const FMaterialTextureSource>(
	    EMaterialTextureEncoding::Linear, std::vector<FMaterialTextureMip>{{1, 1, {255, 255, 255, 255}}});
	return Source;
}
} // namespace

void ValidateMaterialAssetValue(const FMaterialAssetValue& InValue)
{
	(void)ResolveValue(InValue, ValidationTexture, 0);
}

FMaterialValue ResolveMaterialAssetValue(const FMaterialAssetValue& InValue, const FMaterialTextureResolver& InResolve)
{
	return ResolveValue(InValue, InResolve, 0);
}

FMaterialAssetValue PersistMaterialValue(const FMaterialValue& InValue, const FMaterialTextureReference& InReference)
{
	InValue.Validate();
	if (InValue.Type.Kind == EMaterialValueKind::ReadBuffer ||
	    (InValue.Texture && (InValue.Texture->IsRenderTarget() || !InReference)))
	{
		throw std::invalid_argument("Material resource has no persistent asset representation");
	}
	FMaterialAssetValue Result;
	Result.Type = InValue.Type;
	Result.Words = InValue.Words;
	Result.Sampler = InValue.Sampler;
	if (InValue.Texture)
	{
		Result.Texture = InReference(InValue.Texture);
	}
	for (const auto& Element : InValue.Elements)
	{
		Result.Elements.push_back(PersistMaterialValue(Element, InReference));
	}
	ValidateMaterialAssetValue(Result);
	return Result;
}

FMaterialAsset PersistMaterialDescription(const FMaterialDescription& InDescription,
                                          const FMaterialTextureReference& InReference)
{
	FMaterialAsset Result;
	Result.Name = InDescription.Name;
	Result.Version = InDescription.Version;
	Result.Passes = InDescription.Passes;
	for (const auto& Parameter : InDescription.Parameters)
	{
		FMaterialAssetParameter Stored;
		Stored.Name = Parameter.Name;
		Stored.Type = Parameter.Type;
		Stored.Semantic = Parameter.Semantic;
		Stored.Targets = Parameter.Targets;
		Stored.Source = Parameter.Source;
		Stored.OverridePolicy = Parameter.OverridePolicy;
		Stored.OverrideScopes = Parameter.OverrideScopes;
		Stored.bRequired = Parameter.bRequired;
		Stored.bActive = Parameter.bActive;
		if (Parameter.Default)
		{
			Stored.Default = PersistMaterialValue(*Parameter.Default, InReference);
		}
		Result.Parameters.push_back(std::move(Stored));
	}
	ValidateMaterialAsset(Result);
	return Result;
}

FMaterialDescription ResolveMaterialAssetDescription(const FMaterialAsset& InAsset,
                                                     const FMaterialTextureResolver& InResolve)
{
	FMaterialDescription Result;
	Result.Name = InAsset.Name;
	Result.Version = InAsset.Version;
	Result.Passes = InAsset.Passes;
	for (const auto& Stored : InAsset.Parameters)
	{
		FMaterialParameterDeclaration Parameter;
		Parameter.Name = Stored.Name;
		Parameter.Type = Stored.Type;
		Parameter.Semantic = Stored.Semantic;
		Parameter.Targets = Stored.Targets;
		Parameter.Source = Stored.Source;
		Parameter.OverridePolicy = Stored.OverridePolicy;
		Parameter.OverrideScopes = Stored.OverrideScopes;
		Parameter.bRequired = Stored.bRequired;
		Parameter.bActive = Stored.bActive;
		if (Stored.Default)
		{
			Parameter.Default = ResolveMaterialAssetValue(*Stored.Default, InResolve);
		}
		Result.Parameters.push_back(std::move(Parameter));
	}
	return Result;
}

FMaterialParameterValues ResolveMaterialAssetValues(const FMaterialAssetValues& InValues,
                                                    const FMaterialTextureResolver& InResolve)
{
	FMaterialParameterValues Result;
	std::set<std::string> Names;
	for (const auto& Entry : InValues)
	{
		if (Entry.Name.empty() || !Names.insert(Entry.Name).second)
		{
			throw std::invalid_argument("Empty or duplicate persistent material value name");
		}
		Result.push_back({Entry.Name, ResolveMaterialAssetValue(Entry.Value, InResolve)});
	}
	return Result;
}

void ValidateMaterialAsset(const FMaterialAsset& InAsset)
{
	const FMaterialDefinition Definition(ResolveMaterialAssetDescription(InAsset, ValidationTexture));
	for (const auto& Entry : ResolveMaterialAssetValues(InAsset.Values, ValidationTexture))
	{
		const auto& Schema = *Definition.GetSchema();
		ValidateMaterialOverride(Schema.Get(Schema.Find(Entry.Name)), Entry.Value, EMaterialScope::Material);
	}
}
} // namespace Hyperion
