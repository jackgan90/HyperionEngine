#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/Core/ContentHash.h"
#include <array>

namespace Hyperion
{
namespace
{
constexpr std::array<std::byte, 4> Magic{std::byte{'H'}, std::byte{'A'}, std::byte{'S'}, std::byte{'T'}};
constexpr std::uint32_t ContainerVersion = 1;
constexpr std::size_t DigestOffset = Magic.size() + sizeof(std::uint32_t) + sizeof(std::uint64_t);
constexpr std::size_t DigestSize = 64;
constexpr std::size_t HeaderSize = DigestOffset + DigestSize;

void AppendInteger(std::vector<std::byte>& OutBytes, std::uint64_t InValue, unsigned InSize)
{
	for (unsigned Index = 0; Index < InSize; ++Index)
	{
		OutBytes.push_back(std::byte((InValue >> (Index * 8)) & 255));
	}
}

std::uint64_t ReadInteger(std::span<const std::byte> InBytes, std::size_t InOffset, unsigned InSize)
{
	std::uint64_t Value{};
	for (unsigned Index = 0; Index < InSize; ++Index)
	{
		Value |= std::uint64_t(std::to_integer<unsigned>(InBytes[InOffset + Index])) << (Index * 8);
	}
	return Value;
}

void CollectStoredReferences(const FArchiveNode& InNode, std::string_view InPath,
                             std::vector<FAssetDependency>& OutDependencies)
{
	if (const auto* Object = std::get_if<FArchiveNode::FObject>(&InNode.Value))
	{
		const auto Type = Object->find("type");
		const auto Fields = Object->find("fields");
		if (Type != Object->end() && Fields != Object->end() && Object->contains("version") &&
		    std::holds_alternative<std::string>(Type->second.Value) &&
		    std::holds_alternative<FArchiveNode::FObject>(Fields->second.Value))
		{
			if (ReadValue<std::string>(Type->second) == RecordType<FAssetRef>().Id)
			{
				OutDependencies.push_back({std::string(InPath), ReadValue<FAssetRef>(InNode)});
				return;
			}
			for (const auto& [Name, Value] : std::get<FArchiveNode::FObject>(Fields->second.Value))
			{
				CollectStoredReferences(Value, FRecordReadContext{std::string(InPath)}.Child(Name).Path,
				                        OutDependencies);
			}
		}
		else
		{
			for (const auto& [Name, Value] : *Object)
			{
				CollectStoredReferences(Value, std::string(InPath) + "[" + Name + "]", OutDependencies);
			}
		}
	}
	else if (const auto* Array = std::get_if<FArchiveNode::FArray>(&InNode.Value))
	{
		for (std::size_t Index = 0; Index < Array->size(); ++Index)
		{
			CollectStoredReferences((*Array)[Index], std::string(InPath) + "[" + std::to_string(Index) + "]",
			                        OutDependencies);
		}
	}
}

void ValidateDocument(const FAssetDocument& InDocument, FArchiveLimits InLimits)
{
	const auto& Root = std::get<FArchiveNode::FObject>(InDocument.Object.Value);
	if (ReadValue<std::string>(Root.at("type")) != InDocument.Header.TypeId ||
	    ReadValue<std::uint32_t>(Root.at("version")) != InDocument.Header.SchemaVersion)
	{
		throw std::runtime_error("Native asset header/body type or schema mismatch");
	}
	if (HashArchive(InDocument.Object, InLimits) != InDocument.Header.Revision)
	{
		throw std::runtime_error("Native asset content revision mismatch");
	}
	std::vector<FAssetDependency> Dependencies;
	CollectStoredReferences(InDocument.Object, {}, Dependencies);
	std::sort(Dependencies.begin(), Dependencies.end());
	if (Dependencies != InDocument.Header.Dependencies)
	{
		throw std::runtime_error("Native asset dependency table mismatch");
	}
}

FAssetDocument ReadLegacy(std::shared_ptr<const std::vector<std::byte>> InBytes, FArchiveLimits InLimits)
{
	FAssetDocument Result;
	Result.Object = DecodeArchive(InBytes, 0, InLimits);
	Result.bLegacy = true;
	Result.StoredBytes = InBytes->size();
	const auto& Root = std::get<FArchiveNode::FObject>(Result.Object.Value);
	Result.Header.TypeId = ReadValue<std::string>(Root.at("type"));
	Result.Header.SchemaVersion = ReadValue<std::uint32_t>(Root.at("version"));
	Result.Header.Revision = HashArchive(Result.Object, InLimits);
	Result.Header.Id = ContentHash(*InBytes).substr(0, 32);
	CollectStoredReferences(Result.Object, {}, Result.Header.Dependencies);
	std::sort(Result.Header.Dependencies.begin(), Result.Header.Dependencies.end());
	return Result;
}
} // namespace

std::size_t GetNativeAssetPrefixSize()
{
	return HeaderSize;
}

FNativeAssetPayloadRange ProbeNativeAssetPayload(std::span<const std::byte> InPrefix, FArchiveLimits InLimits)
{
	if (InPrefix.size() < HeaderSize || !std::equal(Magic.begin(), Magic.end(), InPrefix.begin()) ||
	    ReadInteger(InPrefix, Magic.size(), sizeof(std::uint32_t)) != ContainerVersion)
	{
		throw std::runtime_error("Invalid or unsupported native asset container header");
	}
	const auto Size = ReadInteger(InPrefix, Magic.size() + sizeof(std::uint32_t), sizeof(std::uint64_t));
	if (InLimits.MaxBytes < HeaderSize || Size > InLimits.MaxBytes - HeaderSize)
	{
		throw std::runtime_error("Invalid native asset size");
	}
	return {HeaderSize, static_cast<std::size_t>(Size)};
}

FEncodedAsset EncodeAsset(const FRecordDescriptor& InType, const void* InObject, FAssetHeader InHeader,
                          FArchiveLimits InLimits)
{
	if (InLimits.MaxBytes < HeaderSize)
	{
		throw std::runtime_error("Native asset size limit is smaller than its header");
	}
	InLimits.MaxBytes -= HeaderSize;
	auto Object = WriteRecord(InType, InObject);
	if (InHeader.Id.empty())
	{
		InHeader.Id = CreateIdentifier();
	}
	InHeader.TypeId = InType.Id;
	InHeader.SchemaVersion = InType.Version;
	InHeader.Revision = HashArchive(Object, InLimits);
	InHeader.Dependencies = CollectAssetDependencies(InType, InObject);
	ValidateAssetHeader(InHeader);
	auto Payload = EncodeArchive(
	    FArchiveNode(FArchiveNode::FObject{{"header", WriteValue(InHeader)}, {"object", std::move(Object)}}), InLimits);
	const auto Digest = ContentHash(Payload);
	FEncodedAsset Result{std::move(InHeader)};
	Result.Bytes.reserve(HeaderSize + Payload.size());
	Result.Bytes.insert(Result.Bytes.end(), Magic.begin(), Magic.end());
	AppendInteger(Result.Bytes, ContainerVersion, sizeof(ContainerVersion));
	AppendInteger(Result.Bytes, Payload.size(), sizeof(std::uint64_t));
	for (char Character : Digest)
	{
		Result.Bytes.push_back(std::byte(Character));
	}
	Result.Bytes.insert(Result.Bytes.end(), Payload.begin(), Payload.end());
	return Result;
}

FAssetDocument DecodeAsset(std::shared_ptr<const std::vector<std::byte>> InBytes, FArchiveLimits InLimits)
{
	if (!InBytes || InBytes->size() < 8 || InBytes->size() > InLimits.MaxBytes)
	{
		throw std::runtime_error("Invalid native asset size");
	}
	if ((*InBytes)[0] == std::byte{'H'} && (*InBytes)[1] == std::byte{'Y'} && (*InBytes)[2] == std::byte{'P'} &&
	    (*InBytes)[3] == std::byte{'A'})
	{
		return ReadLegacy(std::move(InBytes), InLimits);
	}
	const auto Payload = ProbeNativeAssetPayload(*InBytes, InLimits);
	if (Payload.Size != InBytes->size() - Payload.Offset)
	{
		throw std::runtime_error("Invalid or unsupported native asset container header");
	}
	const std::string Digest(reinterpret_cast<const char*>(InBytes->data() + DigestOffset), DigestSize);
	if (Digest != ContentHash(std::span(*InBytes).subspan(Payload.Offset)))
	{
		throw std::runtime_error("Native asset integrity check failed");
	}
	auto Envelope = DecodeArchive(InBytes, Payload.Offset, InLimits);
	auto& Fields = std::get<FArchiveNode::FObject>(Envelope.Value);
	if (Fields.size() != 2)
	{
		throw std::runtime_error("Invalid native asset envelope");
	}
	FAssetDocument Result;
	Result.Header = ReadValue<FAssetHeader>(Fields.at("header"));
	Result.Object = std::move(Fields.at("object"));
	Result.StoredBytes = InBytes->size();
	ValidateDocument(Result, InLimits);
	return Result;
}

FAssetDocument DecodeAsset(std::span<const std::byte> InBytes, FArchiveLimits InLimits)
{
	if (InBytes.size() > InLimits.MaxBytes)
	{
		throw std::runtime_error("Invalid native asset size");
	}
	return DecodeAsset(std::make_shared<const std::vector<std::byte>>(InBytes.begin(), InBytes.end()), InLimits);
}
} // namespace Hyperion
