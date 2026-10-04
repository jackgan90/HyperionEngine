#include "AssetPropertyWidgets.h"
#include "AssetWorkspace.h"
#include "Hyperion/AssetEditing/AssetProperties.h"
#include "MaterialChoices.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace Hyperion
{
namespace
{
bool FitsInlineValue(const FMaterialParameterType& InType, std::size_t& InOutBudget)
{
	if (!InOutBudget)
	{
		return false;
	}
	--InOutBudget;
	if (InType.Kind == EMaterialValueKind::Numeric)
	{
		const auto Words = std::size_t(InType.Rows) * InType.Columns;
		if (Words > InOutBudget)
		{
			return false;
		}
		InOutBudget -= Words;
	}
	else if (InType.Kind == EMaterialValueKind::Array)
	{
		auto ElementBudget = InOutBudget;
		if (!FitsInlineValue(InType.Members.at(0), ElementBudget))
		{
			return false;
		}
		const auto ElementCost = InOutBudget - ElementBudget;
		if (InType.ArrayCount > InOutBudget / ElementCost)
		{
			return false;
		}
		InOutBudget -= InType.ArrayCount * ElementCost;
	}
	else if (InType.Kind == EMaterialValueKind::Structure)
	{
		for (const auto& Member : InType.Members)
		{
			if (!FitsInlineValue(Member, InOutBudget))
			{
				return false;
			}
		}
	}
	return true;
}

FMaterialAssetValue MakeEditableValue(const FMaterialParameterType& InType)
{
	FMaterialAssetValue Value;
	Value.Type = InType;
	if (InType.Kind == EMaterialValueKind::Numeric)
	{
		Value.Words.resize(InType.Rows * InType.Columns);
	}
	else if (InType.Kind == EMaterialValueKind::Structure)
	{
		for (const auto& Member : InType.Members)
		{
			Value.Elements.push_back(MakeEditableValue(Member));
		}
	}
	else if (InType.Kind == EMaterialValueKind::Array)
	{
		Value.Elements.assign(InType.ArrayCount, MakeEditableValue(InType.Members.at(0)));
	}
	return Value;
}

bool EditSampler(FGui& InGui, const std::string& InId, FMaterialSampler& InSampler)
{
	bool bChanged{};
	static const auto Modes = MaterialChoices<EMaterialAddressMode>();
	for (const auto& [Name, Mode] : {std::pair{"U", &InSampler.U}, {"V", &InSampler.V}, {"W", &InSampler.W}})
	{
		bChanged |= AssetEnumCombo(InGui, (std::string("Address ") + Name + "##" + InId).c_str(), Modes, *Mode);
	}
	for (const auto& [Name, Value] : {std::pair{"Min linear", &InSampler.bMinLinear},
	                                  {"Mag linear", &InSampler.bMagLinear},
	                                  {"Mip linear", &InSampler.bMipLinear},
	                                  {"Comparison", &InSampler.bComparison}})
	{
		bChanged |= AssetCheckbox(InGui, (std::string(Name) + "##" + InId).c_str(), *Value);
	}
	std::uint64_t Anisotropy = InSampler.MaxAnisotropy;
	if (AssetInteger(InGui, ("Anisotropy##" + InId).c_str(), Anisotropy))
	{
		InSampler.MaxAnisotropy =
		    static_cast<std::uint32_t>(std::clamp(Anisotropy, std::uint64_t{1}, std::uint64_t{16}));
		if (InSampler.MaxAnisotropy > 1)
		{
			InSampler.bMinLinear = InSampler.bMagLinear = InSampler.bMipLinear = true;
		}
		bChanged = true;
	}
	if (!InSampler.bMinLinear || !InSampler.bMagLinear || !InSampler.bMipLinear)
	{
		InSampler.MaxAnisotropy = 1;
	}
	bChanged |= AssetFloat(InGui, ("LOD bias##" + InId).c_str(), InSampler.MipLodBias);
	bChanged |= AssetFloat(InGui, ("Min LOD##" + InId).c_str(), InSampler.MinLod);
	bChanged |= AssetFloat(InGui, ("Max LOD##" + InId).c_str(), InSampler.MaxLod);
	InSampler.MipLodBias = std::clamp(InSampler.MipLodBias, -16.f, 15.99f);
	static const auto Compares = MaterialChoices<EMaterialSamplerCompare>();
	bChanged |= AssetEnumCombo(InGui, ("Compare##" + InId).c_str(), Compares, InSampler.Compare);
	for (std::size_t I = 0; I < 4; ++I)
	{
		bChanged |= AssetFloat(InGui, ("Border " + std::to_string(I) + "##" + InId).c_str(), InSampler.BorderColor[I]);
	}
	return bChanged;
}

bool EditWord(FGui& InGui, const char* InLabel, EMaterialScalar InScalar, std::uint32_t& InWord)
{
	if (InScalar == EMaterialScalar::Bool)
	{
		bool bValue = InWord != 0;
		if (AssetCheckbox(InGui, InLabel, bValue))
		{
			InWord = bValue ? 1u : 0u;
			return true;
		}
	}
	else if (InScalar == EMaterialScalar::Float)
	{
		float Value = std::bit_cast<float>(InWord);
		if (AssetFloat(InGui, InLabel, Value) && std::isfinite(Value))
		{
			InWord = std::bit_cast<std::uint32_t>(Value);
			return true;
		}
	}
	else if (InScalar == EMaterialScalar::Int)
	{
		std::int64_t Value = std::bit_cast<std::int32_t>(InWord);
		if (AssetInteger(InGui, InLabel, Value))
		{
			InWord = std::bit_cast<std::uint32_t>(
			    static_cast<std::int32_t>(std::clamp(Value, std::int64_t{INT32_MIN}, std::int64_t{INT32_MAX})));
			return true;
		}
	}
	else
	{
		std::uint64_t Value = InWord;
		if (AssetInteger(InGui, InLabel, Value))
		{
			InWord = static_cast<std::uint32_t>(std::min(Value, std::uint64_t{UINT32_MAX}));
			return true;
		}
	}
	return false;
}

bool CanEdit(const FMaterialAssetParameter& InParameter)
{
	return CanEditMaterialParameter(InParameter);
}

std::string MaterialTypeLabel(const FMaterialParameterType& InType)
{
	if (InType.Kind == EMaterialValueKind::Numeric)
	{
		std::string Scalar;
		switch (InType.Scalar)
		{
			case EMaterialScalar::Bool:
				Scalar = "bool";
				break;
			case EMaterialScalar::Int:
				Scalar = "int";
				break;
			case EMaterialScalar::Uint:
				Scalar = "uint";
				break;
			case EMaterialScalar::Float:
				Scalar = "float";
				break;
			default:
				throw std::invalid_argument("Unknown material scalar");
		}
		return Scalar + " " + std::to_string(InType.Rows) + " x " + std::to_string(InType.Columns);
	}
	switch (InType.Kind)
	{
		case EMaterialValueKind::Structure:
			return "Structure";
		case EMaterialValueKind::Array:
			return "Array";
		case EMaterialValueKind::Texture2D:
			return "Texture 2D";
		case EMaterialValueKind::ReadBuffer:
			return "Buffer";
		case EMaterialValueKind::Sampler:
			return "Sampler";
		case EMaterialValueKind::TextureCube:
			return "Texture cube";
		default:
			throw std::invalid_argument("Unknown material value kind");
	}
}

} // namespace

bool FAssetWorkspace::EditMaterialValue(FGui& InGui, const std::string& InId, FMaterialAssetValue& InValue,
                                        EMaterialEditHint InHint)
{
	if (IsMaterialTexture(InValue.Type.Kind))
	{
		auto Reference = InValue.Texture.value_or(FAssetRef{});
		if (EditReference(InGui, ("Texture##" + InId).c_str(), Reference, RecordType<FTextureAsset>().Id))
		{
			InValue.Texture = Reference;
			return true;
		}
		return false;
	}
	if (InValue.Type.Kind == EMaterialValueKind::Sampler)
	{
		return EditSampler(InGui, InId, InValue.Sampler);
	}
	if (!InValue.Elements.empty())
	{
		bool bChanged{};
		for (std::size_t Index = 0; Index < InValue.Elements.size(); ++Index)
		{
			const auto Name =
			    Index < InValue.Type.MemberNames.size() ? InValue.Type.MemberNames[Index] : std::to_string(Index);
			const bool bOpen = InGui.Section((Name + "##" + InId).c_str(), false);
			ObserveProperty(InGui, "element/" + InId + "/" + Name);
			if (bOpen)
			{
				bChanged |= EditMaterialValue(InGui, InId + "/" + Name, InValue.Elements[Index]);
			}
		}
		return bChanged;
	}
	const bool bReadOnly = InValue.Type.Kind != EMaterialValueKind::Numeric || InValue.Type.Rows != 1;
	InGui.BeginDisabled(bReadOnly);
	bool bChanged{};
	if (InValue.Type.Scalar == EMaterialScalar::Uint && InValue.Words.size() == 1 && InHint == EMaterialEditHint::UvSet)
	{
		const std::array<std::string, 2> Choices{"UV0", "UV1"};
		std::size_t Selected = std::min(InValue.Words[0], 1u);
		if (AssetCombo(InGui, ("UV set##" + InId).c_str(), Choices, Selected))
		{
			InValue.Words[0] = static_cast<std::uint32_t>(Selected);
			InGui.EndDisabled();
			return true;
		}
		InGui.EndDisabled();
		return false;
	}
	const bool bColor = InValue.Type.Scalar == EMaterialScalar::Float && InValue.Words.size() >= 3 &&
	                    InHint == EMaterialEditHint::Color;
	if (bColor)
	{
		FVec3 Color{std::bit_cast<float>(InValue.Words[0]), std::bit_cast<float>(InValue.Words[1]),
		            std::bit_cast<float>(InValue.Words[2])};
		if (InGui.InputColor(("Color##" + InId).c_str(), Color))
		{
			InValue.Words[0] = std::bit_cast<std::uint32_t>(Color.X);
			InValue.Words[1] = std::bit_cast<std::uint32_t>(Color.Y);
			InValue.Words[2] = std::bit_cast<std::uint32_t>(Color.Z);
			bChanged = true;
		}
	}
	for (std::size_t I = bColor ? 3 : 0; I < InValue.Words.size(); ++I)
	{
		bChanged |= EditWord(InGui, ("Value " + std::to_string(I) + "##" + InId).c_str(), InValue.Type.Scalar,
		                     InValue.Words[I]);
		ObserveProperty(InGui, "value/" + InId + "/" + std::to_string(I));
	}
	InGui.EndDisabled();
	return bChanged && !bReadOnly;
}

void FAssetWorkspace::DrawMaterialProperties(FGui& InGui, FEntry& InEntry)
{
	auto Material = ReadValue<FMaterialAsset>(InEntry.Document->Snapshot());
	AssetInfo(InGui, "Definition version", std::to_string(Material.Version));
	if (InGui.Section("Shader passes (read-only)", false))
	{
		for (const auto& Pass : Material.Passes)
		{
			AssetInfo(InGui, Pass.Usage.c_str(),
			          Pass.Vertex.Path + ":" + Pass.Vertex.Entry + " | " + Pass.Pixel.Path + ":" + Pass.Pixel.Entry);
		}
	}
	for (const auto& Parameter : Material.Parameters)
	{
		DrawMaterialParameter(InGui, InEntry, Material, Parameter);
	}
}

void FAssetWorkspace::DrawMaterialParameter(FGui& InGui, FEntry& InEntry, FMaterialAsset& InMaterial,
                                            const FMaterialAssetParameter& InParameter)
{
	const auto Override = std::find_if(InMaterial.Values.begin(), InMaterial.Values.end(),
	                                   [&](const auto& InValue)
	                                   {
		                                   return InValue.Name == InParameter.Name;
	                                   });
	const bool bOverridden = Override != InMaterial.Values.end();
	// Count the type description before expanding arrays or copying authored values on Main.
	std::size_t InlineBudget = 1024;
	const bool bFitsInline = FitsInlineValue(InParameter.Type, InlineBudget);
	const bool bEditable = CanEdit(InParameter) && bFitsInline;
	const auto Label = InParameter.Name + (bEditable ? "" : " [read-only]") + "###parameter-" + InParameter.Name;
	if (!InGui.Section(Label.c_str(), bEditable))
	{
		return;
	}
	AssetInfo(InGui, "Source",
	          bOverridden           ? "Values override"
	          : InParameter.Default ? "Declared default"
	                                : "Engine / unset");
	AssetInfo(InGui, "Semantic", InParameter.Semantic);
	AssetInfo(InGui, "Type", MaterialTypeLabel(InParameter.Type));
	if (!bFitsInline)
	{
		InGui.TextWrapped("This parameter is too large for inline editing.");
		return;
	}
	if (!bOverridden && !InParameter.Default && !bEditable)
	{
		InGui.TextWrapped("Value supplied by runtime semantics; no authored value.");
		return;
	}
	FMaterialAssetValue Value;
	if (bOverridden)
	{
		Value = Override->Value;
	}
	else if (InParameter.Default)
	{
		Value = *InParameter.Default;
	}
	else
	{
		Value = MakeEditableValue(InParameter.Type);
	}
	InGui.BeginDisabled(!bEditable);
	FAssetLiveEditScope Editing(InGui);
	const bool bChanged = EditMaterialValue(
	    InGui, InParameter.Name, Value, GetEngineSemanticPolicy(FMaterialSemanticId(InParameter.Semantic)).EditHint);
	const auto Interaction = Editing.Finish();
	if (Interaction.ActiveInteraction)
	{
		InEntry.GuiInteraction = Interaction.ActiveInteraction;
	}
	const bool bReset = InGui.Button(("Reset to default##" + InParameter.Name).c_str(),
	                                 bOverridden && (InParameter.Default || !InParameter.bRequired));
	ObserveProperty(InGui, "reset/" + InParameter.Name);
	InGui.EndDisabled();
	if ((bChanged || bReset) && bEditable)
	{
		if (bReset)
		{
			InMaterial.Values.erase(Override);
		}
		else if (bOverridden)
		{
			Override->Value = std::move(Value);
		}
		else
		{
			InMaterial.Values.push_back({InParameter.Name, std::move(Value)});
		}
		const auto Policy = ResolveAssetFieldPolicy(*InEntry.Document->Loaded().Type, &FMaterialAsset::Values);
		if (InEntry.ReferenceEdit)
		{
			CommitReferenceEdit(InEntry, Policy.Field(), WriteValue(InMaterial.Values));
		}
		else
		{
			CommitAssetField(*InEntry.Document, Policy.Field(), WriteValue(InMaterial.Values),
			                 Interaction.ChangedInteraction);
		}
	}
}
} // namespace Hyperion
