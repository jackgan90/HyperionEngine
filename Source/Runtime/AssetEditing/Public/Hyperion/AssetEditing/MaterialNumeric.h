#pragma once
#include "Hyperion/AssetEditing/AssetProperties.h"

namespace Hyperion
{
struct FMaterialNumericEdit
{
	std::string Name;
	std::vector<double> Values;
};

struct FMaterialNumericInfo
{
	std::string Name;
	FMaterialParameterType Type;
	std::string Semantic;
	bool bEditable{};
	bool bOverridden{};
	std::vector<double> Values;
};

// Numeric leaves use row-major logical order; resource/aggregate parameters retain their typed APIs.
FMaterialNumericInfo DescribeMaterialNumeric(const FMaterialAsset& InMaterial, std::string_view InName);
FMaterialAssetValues PrepareMaterialNumeric(const FMaterialAsset& InMaterial,
                                            const std::vector<FMaterialNumericEdit>& InEdits);
template<> const FRecordDescriptor& RecordType<FMaterialNumericEdit>();
template<> const FRecordDescriptor& RecordType<FMaterialNumericInfo>();
} // namespace Hyperion
