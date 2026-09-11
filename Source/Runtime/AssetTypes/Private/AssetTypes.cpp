#include "Hyperion/AssetTypes/AssetTypes.h"
#include <set>

namespace Hyperion
{
namespace
{
bool IsHex(std::string_view InValue, std::size_t InLength)
{
	return InValue.size() == InLength && std::all_of(InValue.begin(), InValue.end(),
	                                                 [](char InCharacter)
	                                                 {
		                                                 return (InCharacter >= '0' && InCharacter <= '9') ||
		                                                        (InCharacter >= 'a' && InCharacter <= 'f');
	                                                 });
}

void ValidateCatalog(const FAssetCatalog& InCatalog)
{
	std::set<std::string> Ids;
	for (const auto& Reference : InCatalog.Assets)
	{
		ValidateAssetRef(Reference);
		if (Reference.Id.empty() || Reference.Path.empty() || !Ids.insert(Reference.Id).second)
		{
			throw std::runtime_error("Duplicate or incomplete catalog asset identity");
		}
	}
}
} // namespace

bool IsAssetIdentifier(std::string_view InValue)
{
	return IsHex(InValue, 32);
}

bool IsAssetRevision(std::string_view InValue)
{
	return IsHex(InValue, 64);
}

void ValidateAssetRef(const FAssetRef& InReference)
{
	if ((InReference.Id.empty() && InReference.Path.empty()) || InReference.TypeId.empty() ||
	    (!InReference.Id.empty() && !IsAssetIdentifier(InReference.Id)) ||
	    (!InReference.Revision.empty() && !IsAssetRevision(InReference.Revision)))
	{
		throw std::runtime_error("Invalid asset reference identity, location, type or revision");
	}
}

void ValidateAssetHeader(const FAssetHeader& InHeader)
{
	if (!IsAssetIdentifier(InHeader.Id) || InHeader.TypeId.empty() || !InHeader.SchemaVersion ||
	    !IsAssetRevision(InHeader.Revision))
	{
		throw std::runtime_error("Invalid native asset header");
	}
	for (const auto& Dependency : InHeader.Dependencies)
	{
		ValidateAssetRef(Dependency.Reference);
		if (Dependency.Field.empty())
		{
			throw std::runtime_error("Native asset dependency has no field path");
		}
	}
}

std::vector<FAssetDependency> CollectAssetDependencies(const FRecordDescriptor& InType, const void* InObject)
{
	std::vector<FAssetDependency> Result;
	VisitRecord(InType, InObject,
	            [&](const FRecordDescriptor& InRecord, const void* InValue, std::string_view InField)
	            {
		            if (InRecord.CppType == typeid(FAssetRef))
		            {
			            const auto& Reference = *static_cast<const FAssetRef*>(InValue);
			            ValidateAssetRef(Reference);
			            Result.push_back({std::string(InField), Reference});
		            }
	            });
	std::sort(Result.begin(), Result.end());
	return Result;
}

template<> const FRecordDescriptor& RecordType<FAssetRef>()
{
	static const auto Type =
	    MakeRecord<FAssetRef>("hyperion.assetref",
	                          {Member("id", &FAssetRef::Id), Member("path", &FAssetRef::Path),
	                           Member("type", &FAssetRef::TypeId, {true}), Member("revision", &FAssetRef::Revision)},
	                          1, ValidateAssetRef);
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetDependency>()
{
	static const auto Type = MakeRecord<FAssetDependency>(
	    "hyperion.assetdependency",
	    {Member("field", &FAssetDependency::Field, {true}), Member("reference", &FAssetDependency::Reference, {true})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetSource>()
{
	static const auto Type =
	    MakeRecord<FAssetSource>("hyperion.assetsource", {Member("path", &FAssetSource::Path, {true}),
	                                                      Member("fingerprint", &FAssetSource::Fingerprint, {true})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetProvenance>()
{
	static const auto Type = MakeRecord<FAssetProvenance>(
	    "hyperion.assetprovenance",
	    {Member("importer", &FAssetProvenance::Importer, {true}),
	     Member("importerVersion", &FAssetProvenance::ImporterVersion, {true}),
	     Member("settings", &FAssetProvenance::Settings), Member("sources", &FAssetProvenance::Sources, {true}),
	     Member("outputIds", &FAssetProvenance::OutputIds)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetHeader>()
{
	static const auto Type = MakeRecord<FAssetHeader>(
	    "hyperion.assetheader",
	    {Member("id", &FAssetHeader::Id, {true}), Member("type", &FAssetHeader::TypeId, {true}),
	     Member("schema", &FAssetHeader::SchemaVersion, {true}), Member("revision", &FAssetHeader::Revision, {true}),
	     Member("dependencies", &FAssetHeader::Dependencies, {true}), Member("import", &FAssetHeader::Import)},
	    1, ValidateAssetHeader);
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetCatalog>()
{
	static const auto Type = MakeRecord<FAssetCatalog>(
	    "hyperion.assetcatalog", {Member("assets", &FAssetCatalog::Assets, {true})}, 1, ValidateCatalog);
	return Type;
}
} // namespace Hyperion
