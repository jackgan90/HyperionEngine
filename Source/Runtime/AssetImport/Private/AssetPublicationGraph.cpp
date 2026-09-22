#include "AssetPublicationInternal.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Materials/MaterialAsset.h"

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

bool FPublication::PreserveExternal(FAssetRef& InReference)
{
	const auto Path = PathFromUtf8(InReference.Path);
	if (!IsPackagePath(Path))
	{
		return false;
	}
	const auto Normalized = NormalizeFilePath(Path);
	if (const auto Existing = ExternalAssets.find(Normalized); Existing != ExternalAssets.end())
	{
		ValidateAssetRef(InReference);
		const auto& Identity = Existing->second;
		if (Identity.TypeId != InReference.TypeId || (!InReference.Id.empty() && Identity.Id != InReference.Id) ||
		    (!InReference.Revision.empty() && Identity.Revision != InReference.Revision))
		{
			throw std::runtime_error("External asset reference identity/type/revision mismatch: " + InReference.Path);
		}
		InReference = Identity;
		InReference.Revision.clear();
		return true;
	}
	auto& Assets = IndexedAssets();
	const auto Graph = Assets.LoadGraphAsync(InReference, {}).Get(IO.TaskSystem());
	if (!Graph->Failures.empty())
	{
		throw std::runtime_error("External asset dependency is invalid: " + Graph->Failures.front().Error);
	}
	for (const auto& [AssetPath, Asset] : Graph->Assets)
	{
		const auto AssetBytes = IO.ReadAsync(AssetPath, Cancellation).Get(IO.TaskSystem());
		const auto Header = DecodeAsset(AssetBytes).Header;
		if (Header.Id != Asset->Header.Id || Header.TypeId != Asset->Header.TypeId ||
		    Header.Revision != Asset->Header.Revision)
		{
			throw std::runtime_error("External asset changed during dependency validation: " + PathToUtf8(AssetPath));
		}
		const auto Fingerprint = ContentHash(*AssetBytes);
		const auto [It, bInserted] = Sources.emplace(AssetPath, Fingerprint);
		if (!bInserted && It->second != Fingerprint)
		{
			throw std::runtime_error("External asset changed during publication: " + PathToUtf8(AssetPath));
		}
	}
	const auto& RootAsset = *Graph->Root;
	InReference = {RootAsset.Header.Id, PathToUtf8(RootAsset.Path), RootAsset.Header.TypeId, RootAsset.Header.Revision};
	ExternalAssets.emplace(Normalized, InReference);
	InReference.Revision.clear();
	return true;
}

void FPublication::Rewrite(void* InObject, const FRecordDescriptor& InType, const std::filesystem::path& InSource,
                           const std::filesystem::path& InDestination)
{
	VisitRecord(InType, InObject,
	            [&](const FRecordDescriptor& InRecord, const void* InValue, std::string_view InField)
	            {
		            if (InRecord.CppType == typeid(FMaterialShader) && IsPackagePath(InDestination))
		            {
			            auto& Shader = *const_cast<FMaterialShader*>(static_cast<const FMaterialShader*>(InValue));
			            if (!Shader.Path.empty() && !IsPackagePath(PathFromUtf8(Shader.Path)))
			            {
				            Shader.Path = "/Engine/Shaders/" + Shader.Path;
			            }
		            }
		            if (InRecord.CppType != typeid(FAssetRef))
		            {
			            return;
		            }
		            // The visitor receives an exclusively owned mutable clone created below.
		            auto& Reference = *const_cast<FAssetRef*>(static_cast<const FAssetRef*>(InValue));
		            if (PreserveExternal(Reference))
		            {
			            return;
		            }
		            if (Reference.Path.empty())
		            {
			            throw std::runtime_error(std::string(InField) + ": import requires a source path");
		            }
		            const auto Path = Reference.Path.starts_with("@")
		                                  ? ImportProductPath(InSource, Reference.Path.substr(1))
		                                  : ImportPath(InSource.parent_path() / PathFromUtf8(Reference.Path));
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
		            Reference.Path = ImportRelativePath(Existing->second.Path, InDestination.parent_path());
	            });
}

FPublishedAsset FPublication::Build(const std::filesystem::path& InSource, const FConvertedAsset& InAsset, bool bInRoot)
{
	const auto Key = std::make_pair(InSource, InAsset.Type->Id);
	if (Active.size() >= 64 || Published.size() >= 4096 || !Active.insert(Key).second)
	{
		throw std::runtime_error("Import dependency cycle or graph limit at " + ImportPathString(InSource));
	}
	AddProducts(InSource, InAsset);
	Track(InAsset);
	Bytes += InAsset.RetainedBytes;
	if (Bytes > 1024ull * 1024ull * 1024ull)
	{
		throw std::runtime_error("Import graph exceeds 1 GiB retained data budget");
	}
	auto Object = ReadRecord(*InAsset.Type, WriteRecord(*InAsset.Type, InAsset.Object.get()));
	const auto IdentityKey = bInRoot                      ? "$root"
	                         : !InAsset.StableKey.empty() ? InAsset.StableKey
	                         : InAsset.NativeHeader ? "native/" + InAsset.NativeHeader->Id + "|" + InAsset.Type->Id
	                                                : "source/" + StableSourceKey(InSource) + "|" + InAsset.Type->Id;
	auto Id = SelectId(IdentityKey, InAsset, bInRoot);
	auto Destination = bInRoot ? Output : ProductDestination(Id, InAsset);
	Rewrite(Object.get(), *InAsset.Type, InAsset.ProductRoot.empty() ? InSource : InAsset.ProductRoot, Destination);
	std::string TextureContent;
	if (!bInRoot && !InAsset.NativeHeader && InAsset.Type->Id == "hyperion.textureasset")
	{
		auto Record = WriteRecord(*InAsset.Type, Object.get());
		std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Record.Value).at("fields").Value)["name"] =
		    WriteValue(std::string{});
		TextureContent = HashArchive(Record);
		if (!ExistingAssets.contains(Id))
		{
			if (const auto Reused = ReuseTexture(TextureContent))
			{
				ClaimProduct(Reused->Reference.Id, IdentityKey, TextureContent);
				Provenance.OutputIds[IdentityKey] = Reused->Reference.Id;
				Published.emplace(Key, *Reused);
				LibraryProducts[IdentityKey] = Reused->Reference;
				Active.erase(Key);
				return *Reused;
			}
		}
	}
	auto Header = MakeHeader(Id, IdentityKey, TextureContent, bInRoot);
	auto Encoded = EncodeAsset(*InAsset.Type, Object.get(), std::move(Header));
	const auto Path = Destination;
	if (!bInRoot)
	{
		if (const auto [It, bInserted] = ProductRevisions.emplace(IdentityKey, Encoded.Header.Revision);
		    !bInserted && It->second != Encoded.Header.Revision)
		{
			throw std::runtime_error("Conflicting products claim one shared import identity: " + IdentityKey);
		}
		ClaimProduct(Id, IdentityKey, TextureContent.empty() ? Encoded.Header.Revision : TextureContent);
		LibraryProducts[IdentityKey] = {Id, ImportRelativePath(Path, Library), Encoded.Header.TypeId, {}};
	}
	Write(Path, Encoded);
	FPublishedAsset Result{
	    {Id, ImportPathString(Path.filename()), Encoded.Header.TypeId, {}}, Path, SourceIdentity(InAsset)};
	Published.emplace(Key, Result);
	if (!TextureContent.empty())
	{
		TextureProducts[TextureContent] = Result;
	}
	Active.erase(Key);
	if (bInRoot)
	{
		Root = std::move(Encoded);
	}
	return Result;
}
} // namespace Hyperion
