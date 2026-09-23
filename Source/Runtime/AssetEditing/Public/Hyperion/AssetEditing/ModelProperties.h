#pragma once
#include "Hyperion/AssetEditing/AssetDocument.h"

namespace Hyperion
{
struct FModelPrimitiveInfo
{
	std::string Id;
	std::string Name;
	std::int32_t Material = -1;
	std::uint64_t Vertices{};
	std::uint64_t Indices{};
};

std::vector<FModelPrimitiveInfo> DescribeModelPrimitives(const FAssetEditDocument& InDocument);
void SetModelPrimitives(FAssetEditDocument& InDocument, const std::vector<FModelPrimitiveInfo>& InValues,
                        std::uint64_t InInteraction = 0);
void CommitModelPrimitiveFields(FAssetEditDocument& InDocument, const FArchiveNode& InValue,
                                std::uint64_t InInteraction = 0);
template<> const FRecordDescriptor& RecordType<FModelPrimitiveInfo>();
} // namespace Hyperion
