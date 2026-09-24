#include "Hyperion/AssetEditing/MaterialNumeric.h"
#include <bit>
#include <cmath>
#include <limits>
#include <set>

namespace Hyperion
{
namespace
{
const FMaterialAssetParameter& FindNumeric(const FMaterialAsset& InMaterial, std::string_view InName)
{
	const auto Found = std::find_if(InMaterial.Parameters.begin(), InMaterial.Parameters.end(),
	                                [&](const auto& InParameter)
	                                {
		                                return InParameter.Name == InName;
	                                });
	if (Found == InMaterial.Parameters.end() || Found->Type.Kind != EMaterialValueKind::Numeric)
	{
		throw std::invalid_argument("Expected an existing numeric material parameter: " + std::string(InName));
	}
	return *Found;
}

std::uint32_t EncodeNumber(EMaterialScalar InScalar, double InValue)
{
	if (!std::isfinite(InValue))
	{
		throw std::invalid_argument("Material number must be finite");
	}
	if (InScalar == EMaterialScalar::Float)
	{
		if (std::abs(InValue) > std::numeric_limits<float>::max())
		{
			throw std::invalid_argument("Material number exceeds float32 range");
		}
		return std::bit_cast<std::uint32_t>(static_cast<float>(InValue));
	}
	const double Minimum = InScalar == EMaterialScalar::Int ? std::numeric_limits<std::int32_t>::min() : 0.0;
	const double Maximum = InScalar == EMaterialScalar::Bool  ? 1.0
	                       : InScalar == EMaterialScalar::Int ? std::numeric_limits<std::int32_t>::max()
	                                                          : std::numeric_limits<std::uint32_t>::max();
	if (std::trunc(InValue) != InValue || InValue < Minimum || InValue > Maximum)
	{
		throw std::invalid_argument("Material integer must be in scalar range; booleans use 0 or 1");
	}
	return InScalar == EMaterialScalar::Int ? std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(InValue))
	                                        : static_cast<std::uint32_t>(InValue);
}
} // namespace

FMaterialNumericInfo DescribeMaterialNumeric(const FMaterialAsset& InMaterial, std::string_view InName)
{
	const auto& Parameter = FindNumeric(InMaterial, InName);
	FMaterialNumericInfo Result{Parameter.Name, Parameter.Type, Parameter.Semantic,
	                            CanEditMaterialParameter(Parameter)};
	const auto Override = std::find_if(InMaterial.Values.begin(), InMaterial.Values.end(),
	                                   [&](const auto& InValue)
	                                   {
		                                   return InValue.Name == InName;
	                                   });
	Result.bOverridden = Override != InMaterial.Values.end();
	const auto* Value = Result.bOverridden ? &Override->Value : Parameter.Default ? &*Parameter.Default : nullptr;
	if (!Value)
	{
		throw std::invalid_argument("Numeric parameter has no override or default");
	}
	for (const auto Word : Value->Words)
	{
		Result.Values.push_back(
		    Parameter.Type.Scalar == EMaterialScalar::Float ? static_cast<double>(std::bit_cast<float>(Word))
		    : Parameter.Type.Scalar == EMaterialScalar::Int ? static_cast<double>(std::bit_cast<std::int32_t>(Word))
		                                                    : static_cast<double>(Word));
	}
	return Result;
}

FMaterialAssetValues PrepareMaterialNumeric(const FMaterialAsset& InMaterial,
                                            const std::vector<FMaterialNumericEdit>& InEdits)
{
	if (InEdits.empty() || InEdits.size() > 100)
	{
		throw std::invalid_argument("Edit 1-100 distinct numeric parameters");
	}
	auto Values = InMaterial.Values;
	std::set<std::string> Names;
	for (const auto& Edit : InEdits)
	{
		const auto& Parameter = FindNumeric(InMaterial, Edit.Name);
		if (!Names.insert(Edit.Name).second || !CanEditMaterialParameter(Parameter) ||
		    Edit.Values.size() != std::size_t(Parameter.Type.Rows) * Parameter.Type.Columns)
		{
			throw std::invalid_argument("Duplicate, read-only or incorrectly sized numeric parameter: " + Edit.Name);
		}
		FMaterialAssetValue Value;
		Value.Type = Parameter.Type;
		for (const auto Number : Edit.Values)
		{
			Value.Words.push_back(EncodeNumber(Value.Type.Scalar, Number));
		}
		ClampEditableMaterialParameter(Parameter.Semantic, Value);
		const auto Found = std::find_if(Values.begin(), Values.end(),
		                                [&](const auto& InValue)
		                                {
			                                return InValue.Name == Edit.Name;
		                                });
		if (Found == Values.end())
		{
			Values.push_back({Edit.Name, std::move(Value)});
		}
		else
		{
			Found->Value = std::move(Value);
		}
	}
	auto Candidate = InMaterial;
	Candidate.Values = Values;
	ValidateMaterialAsset(Candidate);
	return Values;
}

template<> const FRecordDescriptor& RecordType<FMaterialNumericEdit>()
{
	static const auto Type = MakeRecord<FMaterialNumericEdit>(
	    "hyperion.material.numeric.edit",
	    {Member("name", &FMaterialNumericEdit::Name,
	            {.bRequired = true, .Description = "Exact numeric parameter name from material.parameters.get."}),
	     Member("values", &FMaterialNumericEdit::Values,
	            {.bRequired = true,
	             .Description = "Finite numbers in row-major logical order, rows*columns elements. Float32, int32 or "
	                            "uint32 according to the declared scalar; boolean numbers are 0/1. Normalized PBR "
	                            "semantics clamp to 0..1, as in GUI."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialNumericInfo>()
{
	static const auto Type = MakeRecord<FMaterialNumericInfo>(
	    "hyperion.material.numeric.info",
	    {Member("name", &FMaterialNumericInfo::Name), Member("type", &FMaterialNumericInfo::Type),
	     Member("semantic", &FMaterialNumericInfo::Semantic),
	     Member("editable", &FMaterialNumericInfo::bEditable,
	            {.Description = "Parameter override policy; asset.info readOnly and pending-edit state still apply."}),
	     Member("overridden", &FMaterialNumericInfo::bOverridden),
	     Member("values", &FMaterialNumericInfo::Values,
	            {.Description = "Effective override or default in row-major logical order, decoded as numbers."})});
	return Type;
}
} // namespace Hyperion
