#include "Hyperion/AssetEditing/AssetProperties.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace Hyperion
{
bool CanEditMaterialParameter(const FMaterialAssetParameter& InParameter)
{
	return InParameter.bActive && InParameter.Source == EMaterialParameterSource::Manual &&
	       InParameter.OverridePolicy == EMaterialOverridePolicy::AllowOverride &&
	       (InParameter.OverrideScopes & MaterialScopeBit(EMaterialScope::Material)) &&
	       InParameter.Semantic != "Pbr.AlphaMode" && InParameter.Semantic != "Pbr.DoubleSided" &&
	       InParameter.Semantic != "Pbr.Unlit";
}

void ClampEditableMaterialParameter(std::string_view InSemantic, FMaterialAssetValue& InValue)
{
	if (InValue.Type.Kind != EMaterialValueKind::Numeric || InValue.Type.Scalar != EMaterialScalar::Float)
	{
		return;
	}
	const bool bNormalized = InSemantic == "Pbr.MetallicFactor" || InSemantic == "Pbr.RoughnessFactor" ||
	                         InSemantic == "Pbr.OcclusionStrength" || InSemantic == "Pbr.AlphaCutoff" ||
	                         InSemantic == "Pbr.BaseColorFactor";
	for (auto& Word : InValue.Words)
	{
		const auto Value = std::bit_cast<float>(Word);
		if (!std::isfinite(Value))
		{
			throw std::invalid_argument("Material values must be finite");
		}
		if (bNormalized)
		{
			Word = std::bit_cast<std::uint32_t>(std::clamp(Value, 0.f, 1.f));
		}
	}
}

namespace
{
void CollectTextureRequirements(const FMaterialAssetValue& InValue, const FMaterialAssetValue* InBefore,
                                std::vector<FAssetReferenceRequirement>& OutReferences)
{
	if (InValue.Texture && (!InBefore || InBefore->Texture != InValue.Texture))
	{
		OutReferences.push_back({*InValue.Texture, InValue.Type.Kind == EMaterialValueKind::TextureCube
		                                               ? ETextureDimension::Cube
		                                               : ETextureDimension::Texture2D});
	}
	for (std::size_t Index = 0; Index < InValue.Elements.size(); ++Index)
	{
		CollectTextureRequirements(InValue.Elements[Index],
		                           InBefore && Index < InBefore->Elements.size() ? &InBefore->Elements[Index] : nullptr,
		                           OutReferences);
	}
}

FPreparedAssetField PrepareMaterialValues(const FAssetEditDocument& InDocument, const FArchiveNode& InValue)
{
	auto Material = ReadValue<FMaterialAsset>(InDocument.Snapshot());
	auto Values = ReadValue<FMaterialAssetValues>(InValue);
	FPreparedAssetField Result;
	for (const auto& Parameter : Material.Parameters)
	{
		const auto Before = std::find_if(Material.Values.begin(), Material.Values.end(),
		                                 [&](const auto& InEntry)
		                                 {
			                                 return InEntry.Name == Parameter.Name;
		                                 });
		const auto After = std::find_if(Values.begin(), Values.end(),
		                                [&](const auto& InEntry)
		                                {
			                                return InEntry.Name == Parameter.Name;
		                                });
		const bool bChanged =
		    (Before == Material.Values.end()) != (After == Values.end()) ||
		    (Before != Material.Values.end() && After != Values.end() && Before->Value != After->Value);
		if (bChanged && !CanEditMaterialParameter(Parameter))
		{
			throw std::invalid_argument("Material parameter is read-only: " + Parameter.Name);
		}
		if (bChanged && After != Values.end())
		{
			ClampEditableMaterialParameter(Parameter.Semantic, After->Value);
			CollectTextureRequirements(After->Value, Before == Material.Values.end() ? nullptr : &Before->Value,
			                           Result.References);
		}
	}
	Material.Values = std::move(Values);
	ValidateMaterialAsset(Material);
	Result.Value = WriteValue(Material.Values);
	return Result;
}
} // namespace

FPreparedAssetField PrepareAssetField(const FAssetEditDocument& InDocument, std::string_view InField,
                                      FArchiveNode InValue)
{
	FPreparedAssetField Result{std::move(InValue)};
	if (InField == "name")
	{
		(void)ReadValue<std::string>(Result.Value);
	}
	else if (InDocument.Loaded().Type->CppType == typeid(FModelAsset) && InField == "nodes")
	{
		const auto Nodes = ReadValue<std::vector<FModelNode>>(Result.Value);
		const auto Before = ReadValue<std::vector<FModelNode>>(InDocument.Get("nodes"));
		if (Nodes.size() != Before.size())
		{
			throw std::invalid_argument("Model node editing preserves topology");
		}
		for (std::size_t Index = 0; Index < Nodes.size(); ++Index)
		{
			if (Nodes[Index].Id != Before[Index].Id || Nodes[Index].Children != Before[Index].Children ||
			    Nodes[Index].Primitives != Before[Index].Primitives)
			{
				throw std::invalid_argument("Model node identity and topology are read-only");
			}
		}
		ValidateNodeHierarchy(Nodes);
	}
	else if (InDocument.Loaded().Type->CppType == typeid(FModelAsset) && InField == "materialSlots")
	{
		const auto Slots = ReadValue<std::vector<FAssetRef>>(Result.Value);
		const auto Before = ReadValue<std::vector<FAssetRef>>(InDocument.Get("materialSlots"));
		if (Slots.size() != Before.size())
		{
			throw std::invalid_argument("Material slot count is fixed");
		}
		for (std::size_t Index = 0; Index < Slots.size(); ++Index)
		{
			ValidateAssetRef(Slots[Index]);
			if (Slots[Index].TypeId != RecordType<FMaterialAsset>().Id)
			{
				throw std::invalid_argument("Model slots require material references");
			}
			if (Slots[Index] != Before[Index])
			{
				Result.References.push_back({Slots[Index], {}});
			}
		}
	}
	else if (InDocument.Loaded().Type->CppType == typeid(FMaterialAsset) && InField == "values")
	{
		return PrepareMaterialValues(InDocument, Result.Value);
	}
	else
	{
		throw std::invalid_argument("Field requires its dedicated asset operation");
	}
	return Result;
}

void ValidateAssetReferenceGraph(const FAssetGraph& InGraph, std::string_view InType,
                                 std::optional<ETextureDimension> InDimension)
{
	if (!InGraph.Root || !InGraph.Failures.empty() || InGraph.Root->Header.TypeId != InType)
	{
		throw std::invalid_argument("Selected asset has invalid dependencies or a different type");
	}
	if (InDimension && InGraph.Root->As<FTextureAsset>()->Dimension != *InDimension)
	{
		throw std::invalid_argument("Texture dimension does not match the material parameter");
	}
}

void CommitAssetField(FAssetEditDocument& InDocument, std::string InField, FArchiveNode InValue,
                      std::uint64_t InInteraction, bool bInAffectsPreview)
{
	auto Prepared = PrepareAssetField(InDocument, InField, std::move(InValue));
	InDocument.Set(std::move(InField), std::move(Prepared.Value), InInteraction, bInAffectsPreview);
}
} // namespace Hyperion
