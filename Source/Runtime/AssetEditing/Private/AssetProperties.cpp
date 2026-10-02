#include "Hyperion/AssetEditing/AssetProperties.h"
#include "AssetFieldPolicy.h"
#include "Hyperion/Materials/PbrParameters.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace Hyperion
{
bool CanEditMaterialParameter(const FMaterialAssetParameter& InParameter)
{
	const FMaterialSemanticId Semantic(InParameter.Semantic);
	return InParameter.bActive && InParameter.Source == EMaterialParameterSource::Manual &&
	       InParameter.OverridePolicy == EMaterialOverridePolicy::AllowOverride &&
	       (InParameter.OverrideScopes & MaterialScopeBit(EMaterialScope::Material)) &&
	       Semantic != EHyperionMaterialV1Field::AlphaMode && Semantic != EHyperionMaterialV1Field::bDoubleSided &&
	       Semantic != EHyperionMaterialV1Field::bUnlit;
}

void ClampEditableMaterialParameter(std::string_view InSemantic, FMaterialAssetValue& InValue)
{
	if (InValue.Type.Kind != EMaterialValueKind::Numeric || InValue.Type.Scalar != EMaterialScalar::Float)
	{
		return;
	}
	const FMaterialSemanticId Semantic(InSemantic);
	const bool bNormalized =
	    Semantic == EHyperionMaterialV1Field::Metallic || Semantic == EHyperionMaterialV1Field::Roughness ||
	    Semantic == EHyperionMaterialV1Field::OcclusionStrength || Semantic == EHyperionMaterialV1Field::AlphaCutoff ||
	    Semantic == EHyperionMaterialV1Field::BaseColor;
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
                      std::uint64_t InInteraction)
{
	const auto Policy = ResolveAssetFieldPolicy(*InDocument.Loaded().Type, InField);
	CommitAssetField(InDocument, Policy.Field(), std::move(InValue), InInteraction);
}

void CommitAssetField(FAssetEditDocument& InDocument, const FRecordMemberIdentity& InField, FArchiveNode InValue,
                      std::uint64_t InInteraction)
{
	auto Prepared = PrepareAssetField(InDocument, InField, std::move(InValue));
	if (!Prepared.References.empty())
	{
		throw std::invalid_argument("Changed asset references require the shared asynchronous workflow");
	}
	InDocument.Apply(InField, std::move(Prepared.Value), InInteraction);
}
} // namespace Hyperion
