#include "AssetPublicationInternal.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
namespace
{
std::optional<FAssetRef> SourceIdentity(const FConvertedAsset& InAsset)
{
	if (!InAsset.NativeHeader)
	{
		return {};
	}
	const auto& Header = *InAsset.NativeHeader;
	return FAssetRef{Header.Id, {}, Header.TypeId, Header.Revision};
}

void ValidateSourceReference(const FAssetRef& InReference, const std::optional<FAssetRef>& InSourceIdentity,
                             std::string_view InField, const std::filesystem::path& InSource)
{
	if ((!InReference.Id.empty() || !InReference.Revision.empty()) &&
	    (!InSourceIdentity || InSourceIdentity->TypeId != InReference.TypeId ||
	     (!InReference.Id.empty() && InSourceIdentity->Id != InReference.Id) ||
	     (!InReference.Revision.empty() && InSourceIdentity->Revision != InReference.Revision)))
	{
		throw std::runtime_error(ImportPathString(InSource) + ":" + std::string(InField) +
		                         ": source asset reference identity/type/revision mismatch");
	}
}
} // namespace

void FPublication::Rewrite(void* InObject, const FRecordDescriptor& InType, const std::filesystem::path& InSource,
                           const std::filesystem::path& InDestination)
{
	VisitRecord(InType, InObject,
	            [&](const FRecordDescriptor& InRecord, const void* InValue, std::string_view InField)
	            {
		            if (InRecord.CppType != typeid(FAssetRef))
		            {
			            return;
		            }
		            // The visitor receives an exclusively owned mutable clone created below.
		            auto& Reference = *const_cast<FAssetRef*>(static_cast<const FAssetRef*>(InValue));
		            if (Reference.Path.empty())
		            {
			            throw std::runtime_error(std::string(InField) + ": import requires a source path");
		            }
		            const auto Path = ImportPath(InSource.parent_path() / PathFromUtf8(Reference.Path));
		            const auto Key = std::make_pair(Path, Reference.TypeId);
		            auto Existing = Published.find(Key);
		            if (Existing == Published.end())
		            {
			            auto Cached = Converted.find(Key);
			            auto Child = Cached != Converted.end() ? Cached->second : Convert(Path, Reference.TypeId);
			            ValidateSourceReference(Reference, SourceIdentity(Child), InField, InSource);
			            Build(Path, Child, false);
			            Existing = Published.find(Key);
		            }
		            else
		            {
			            ValidateSourceReference(Reference, Existing->second.SourceIdentity, InField, InSource);
		            }
		            Reference = Existing->second.Reference;
		            Reference.Path =
		                ImportPathString(Existing->second.Path.lexically_relative(InDestination.parent_path()));
	            });
}

FPublishedAsset FPublication::Build(const std::filesystem::path& InSource, const FConvertedAsset& InAsset, bool bInRoot)
{
	const auto Key = std::make_pair(InSource, InAsset.Type->Id);
	if (Active.size() >= 64 || Published.size() >= 4096 || !Active.insert(Key).second)
	{
		throw std::runtime_error("Import dependency cycle or graph limit at " + ImportPathString(InSource));
	}
	Track(InAsset);
	Bytes += InAsset.RetainedBytes;
	if (Bytes > 1024ull * 1024ull * 1024ull)
	{
		throw std::runtime_error("Import graph exceeds 1 GiB retained data budget");
	}
	auto Object = ReadRecord(*InAsset.Type, WriteRecord(*InAsset.Type, InAsset.Object.get()));
	const auto IdentityKey =
	    bInRoot ? "$root" : ImportRelativePath(InSource, Source.parent_path()) + "|" + InAsset.Type->Id;
	std::string Id;
	if (const auto It = PreviousIds.find(IdentityKey); It != PreviousIds.end())
	{
		Id = It->second;
	}
	else if (bInRoot && Previous)
	{
		Id = Previous->Header.Id;
	}
	else if (InAsset.NativeHeader)
	{
		Id = InAsset.NativeHeader->Id;
	}
	else
	{
		Id = CreateIdentifier();
	}
	Provenance.OutputIds[IdentityKey] = Id;
	const auto Destination = bInRoot ? Output : Output.parent_path() / ".assets" / (Id + ".hasset");
	Rewrite(Object.get(), *InAsset.Type, InSource, Destination);
	FAssetHeader Header;
	Header.Id = Id;
	if (bInRoot)
	{
		CheckSources();
		for (const auto& [Path, Fingerprint] : Sources)
		{
			Provenance.Sources.push_back({ImportRelativePath(Path, Source.parent_path()), Fingerprint});
		}
		Header.Import = Provenance;
	}
	auto Encoded = EncodeAsset(*InAsset.Type, Object.get(), std::move(Header));
	const auto Path = bInRoot ? Output : Destination.parent_path() / (Id + "-" + Encoded.Header.Revision + ".hasset");
	Write(Path, Encoded);
	FPublishedAsset Result{{Id, ImportPathString(Path.filename()), Encoded.Header.TypeId, Encoded.Header.Revision},
	                       Path,
	                       SourceIdentity(InAsset)};
	Published.emplace(Key, Result);
	Active.erase(Key);
	if (bInRoot)
	{
		Root = std::move(Encoded);
	}
	return Result;
}
} // namespace Hyperion
