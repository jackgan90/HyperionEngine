#pragma once
#include "Hyperion/Content/ContentRootService.h"

namespace Hyperion
{
struct FContentDirectoryQuery
{
	std::uint64_t Generation{};
	std::string Directory = "/Game";
	std::uint32_t Offset{};
	std::uint32_t Limit = 50;
};

struct FContentCandidate
{
	std::string Path;
	std::string State;
	std::string Error;
	std::optional<FAssetRef> Asset;
};

struct FContentDirectoryPage
{
	std::uint64_t Generation{};
	std::uint64_t Total{};
	std::optional<std::uint32_t> Next;
	std::vector<FContentCandidate> Entries;
};

FContentDirectoryPage QueryContentDirectory(const FAssetService& InAssets, const FContentRootService& InRoots,
                                            const FContentDirectoryQuery& InRequest);
template<> const FRecordDescriptor& RecordType<FContentDirectoryQuery>();
template<> const FRecordDescriptor& RecordType<FContentCandidate>();
template<> const FRecordDescriptor& RecordType<FContentDirectoryPage>();

struct FContentAssetQuery
{
	std::uint64_t Generation{};
	std::string Query;
	std::string Type;
	std::uint32_t Offset{};
	std::uint32_t Limit = 50;
};

struct FContentAssetPage
{
	std::uint64_t Generation{};
	std::uint64_t Total{};
	std::optional<std::uint32_t> Next;
	std::vector<FAssetRef> Assets;
};

FContentAssetPage QueryContentAssets(const FAssetService& InAssets, const FContentRootService& InRoots,
                                     const FContentAssetQuery& InRequest);
template<> const FRecordDescriptor& RecordType<FContentAssetQuery>();
template<> const FRecordDescriptor& RecordType<FContentAssetPage>();
} // namespace Hyperion
