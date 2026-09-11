#pragma once
#include "Hyperion/Reflection/Record.h"

namespace Hyperion
{
struct FAssetRef
{
	std::string Id;
	std::string Path;
	std::string TypeId;
	std::string Revision;

	auto operator<=>(const FAssetRef&) const = default;
};

struct FAssetDependency
{
	std::string Field;
	FAssetRef Reference;

	auto operator<=>(const FAssetDependency&) const = default;
};

struct FAssetSource
{
	std::string Path;
	std::string Fingerprint;

	auto operator<=>(const FAssetSource&) const = default;
};

struct FAssetProvenance
{
	std::string Importer;
	std::uint32_t ImporterVersion{};
	std::map<std::string, std::string> Settings;
	std::vector<FAssetSource> Sources;
	std::map<std::string, std::string> OutputIds;
};

struct FAssetHeader
{
	std::string Id;
	std::string TypeId;
	std::uint32_t SchemaVersion{};
	std::string Revision;
	std::vector<FAssetDependency> Dependencies;
	std::optional<FAssetProvenance> Import;
};

struct FAssetCatalog
{
	std::vector<FAssetRef> Assets;
};

bool IsAssetIdentifier(std::string_view InValue);
bool IsAssetRevision(std::string_view InValue);
void ValidateAssetRef(const FAssetRef& InReference);
void ValidateAssetHeader(const FAssetHeader& InHeader);
std::vector<FAssetDependency> CollectAssetDependencies(const FRecordDescriptor& InType, const void* InObject);

template<> const FRecordDescriptor& RecordType<FAssetRef>();
template<> const FRecordDescriptor& RecordType<FAssetDependency>();
template<> const FRecordDescriptor& RecordType<FAssetSource>();
template<> const FRecordDescriptor& RecordType<FAssetProvenance>();
template<> const FRecordDescriptor& RecordType<FAssetHeader>();
template<> const FRecordDescriptor& RecordType<FAssetCatalog>();
} // namespace Hyperion
