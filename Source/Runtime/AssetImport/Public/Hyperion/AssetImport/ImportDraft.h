#pragma once
#include "Hyperion/AssetEditing/MaterialNumeric.h"
#include "Hyperion/AssetEditing/ModelProperties.h"
#include "Hyperion/AssetImport/AssetImportService.h"
#include "Hyperion/Scene/Model.h"
#include "Hyperion/Textures/TextureAsset.h"

namespace Hyperion
{
inline constexpr std::uint32_t ImportDraftPreviewPageLimit = 32;
inline constexpr std::uint32_t ImportDraftMaxPageLimit = 64;

struct FImportNodeEdit
{
	std::string Id;
	std::optional<std::string> Name;
	std::optional<FMat4> Local;
};

struct FImportPrimitiveEdit
{
	std::string Id;
	std::optional<std::string> Name;
	std::optional<std::int32_t> Material;
};

struct FImportPropertyEdits
{
	std::optional<std::string> Name;
	std::vector<FImportNodeEdit> Nodes;
	std::vector<FImportPrimitiveEdit> Primitives;
	std::vector<FMaterialNumericEdit> Material;
};

struct FImportDraftQuery
{
	std::string Draft;
	std::uint32_t Offset{};
	std::uint32_t Limit = ImportDraftPreviewPageLimit;
};

struct FImportDraftList
{
	std::vector<std::string> Drafts;
};

struct FImportDraftMutation
{
	std::string Draft;
	std::uint64_t Generation{};
};

struct FImportDraftEdit
{
	std::string Draft;
	std::uint64_t Generation{};
	FImportPropertyEdits Properties;
};

struct FImportDraftHistory
{
	std::string Draft;
	std::uint64_t Generation{};
	std::string Action;
};

struct FImportDraftDiscard
{
	std::string Draft;
	std::uint64_t Generation{};
	bool bDiscard{};
};

struct FImportProductInfo
{
	std::string Name;
	std::string Type;
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::uint32_t Mips{};
	std::string Format;
};

struct FImportDraftInfo
{
	std::string Draft;
	std::uint64_t Generation{};
	std::string Status;
	std::string Error;
	std::string Type;
	std::string Name;
	bool bNameEditable{};
	bool bDirty{};
	bool bCanUndo{};
	bool bCanRedo{};
	FImportPropertyEdits Properties;
	std::vector<FModelNode> Nodes;
	std::vector<FModelPrimitiveInfo> Primitives;
	std::vector<FAssetRef> MaterialSlots;
	std::vector<FMaterialNumericInfo> Material;
	std::vector<FImportProductInfo> Products;
	std::vector<FAssetDependency> Dependencies;
	std::vector<std::string> Diagnostics;
	std::uint32_t TotalNodes{};
	std::uint32_t TotalPrimitives{};
	std::uint32_t TotalMaterial{};
	std::uint32_t TotalProducts{};
	std::uint32_t TotalDependencies{};
	std::uint32_t TotalMaterialSlots{};
	std::uint32_t TotalDiagnostics{};
	std::uint64_t Vertices{};
	std::uint64_t Triangles{};
	FVec3 BoundsMin;
	FVec3 BoundsMax;
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::uint32_t Mips{};
	std::string Format;
	std::string Encoding;
	std::vector<std::string> Details;
	std::uint32_t SourceWidth{};
	std::uint32_t SourceHeight{};
	std::optional<FEnvironmentBakeSettings> Sky;
	ETextureDimension Dimension = ETextureDimension::Texture2D;
	std::uint64_t PixelBytes{};
};

FConvertedAsset ApplyImportProperties(const FConvertedAsset& InSource, const FImportPropertyEdits& InEdits);
std::string ImportPropertiesKey(const FImportPropertyEdits& InEdits);
void DescribeImportRoot(FImportDraftInfo& OutInfo, const FConvertedAsset& InRoot, const FImportDraftQuery& InQuery);

template<> const FRecordDescriptor& RecordType<FImportNodeEdit>();
template<> const FRecordDescriptor& RecordType<FImportPrimitiveEdit>();
template<> const FRecordDescriptor& RecordType<FImportPropertyEdits>();
template<> const FRecordDescriptor& RecordType<FImportDraftQuery>();
template<> const FRecordDescriptor& RecordType<FImportDraftList>();
template<> const FRecordDescriptor& RecordType<FImportDraftMutation>();
template<> const FRecordDescriptor& RecordType<FImportDraftEdit>();
template<> const FRecordDescriptor& RecordType<FImportDraftHistory>();
template<> const FRecordDescriptor& RecordType<FImportDraftDiscard>();
template<> const FRecordDescriptor& RecordType<FImportProductInfo>();
template<> const FRecordDescriptor& RecordType<FImportDraftInfo>();
} // namespace Hyperion
