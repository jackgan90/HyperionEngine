#include "ArchiveInternal.h"

namespace Hyperion::Private
{
namespace
{
struct FArchivePrefix
{
	std::uint32_t Version{};
	std::uint64_t MetadataBytes{};
	std::uint32_t BlockCount{};
};

FArchivePrefix ReadArchivePrefix(std::span<const std::byte> InBytes, FArchiveLimits InLimits)
{
	if (InBytes.size() < ArchiveVersionSize || InBytes.size() > InLimits.MaxBytes ||
	    !std::equal(ArchiveMagic.begin(), ArchiveMagic.end(), InBytes.begin()))
	{
		throw std::runtime_error("Invalid Hyperion archive header/size");
	}
	FArchiveReader Reader{InLimits, InBytes, {}, 0, ArchiveMagic.size(), InBytes.size()};
	FArchivePrefix Result;
	Result.Version = Reader.Scalar<std::uint32_t>();
	if (Result.Version == LegacyArchiveVersion)
	{
		return Result;
	}
	if (Result.Version != ArchiveVersion)
	{
		throw std::runtime_error("Unsupported archive container version");
	}
	Result.MetadataBytes = Reader.Scalar<std::uint64_t>();
	Result.BlockCount = Reader.Scalar<std::uint32_t>();
	if (Reader.Scalar<std::uint32_t>() != 0)
	{
		throw std::runtime_error("Invalid archive block directory");
	}
	return Result;
}

std::size_t ResolveArchiveMetadataSize(const FArchivePrefix& InPrefix, std::size_t InTotalBytes,
                                       FArchiveLimits InLimits)
{
	if (InPrefix.Version != ArchiveVersion)
	{
		throw std::runtime_error("Unsupported archive container version");
	}
	if (InTotalBytes < ArchivePrefixSize || InTotalBytes > InLimits.MaxBytes)
	{
		throw std::runtime_error("Invalid archive total length");
	}
	if (InPrefix.BlockCount > InLimits.MaxNodes ||
	    InPrefix.BlockCount > (InTotalBytes - ArchivePrefixSize) / ArchiveDirectoryEntrySize)
	{
		throw std::runtime_error("Invalid archive block directory");
	}
	const auto MetadataOffset = ArchivePrefixSize + std::size_t(InPrefix.BlockCount) * ArchiveDirectoryEntrySize;
	if (InPrefix.MetadataBytes > InTotalBytes - MetadataOffset)
	{
		throw std::runtime_error("Invalid archive metadata length");
	}
	return MetadataOffset + static_cast<std::size_t>(InPrefix.MetadataBytes);
}
} // namespace

void FArchiveReader::Charge(std::size_t InBytes)
{
	if (InBytes > Limits.MaxAllocatedBytes - Allocated)
	{
		throw std::runtime_error("Archive allocation budget exceeded");
	}
	Allocated += InBytes;
}

std::span<const std::byte> FArchiveReader::Raw(std::size_t InSize)
{
	if (Position > End || InSize > End - Position)
	{
		throw std::runtime_error("Truncated archive");
	}
	const auto Result = Bytes.subspan(Position, InSize);
	Position += InSize;
	return Result;
}

std::string FArchiveReader::String()
{
	const auto Data = Raw(Scalar<std::uint32_t>());
	Charge(Data.size());
	return {reinterpret_cast<const char*>(Data.data()), Data.size()};
}

void FArchiveReader::Header()
{
	const auto Prefix = ReadArchivePrefix(Bytes, Limits);
	Position = ArchiveVersionSize;
	if (Prefix.Version == LegacyArchiveVersion && !bMetadataOnly)
	{
		bLegacy = true;
		return;
	}
	const auto Total = bMetadataOnly ? TotalBytes : Bytes.size();
	const auto MetadataEnd = ResolveArchiveMetadataSize(Prefix, Total, Limits);
	if (MetadataEnd > Bytes.size())
	{
		throw std::runtime_error("Invalid archive metadata length");
	}
	Position = ArchivePrefixSize;
	Charge(std::size_t(Prefix.BlockCount) * sizeof(Blocks.front()));
	auto Next = MetadataEnd;
	Blocks.reserve(Prefix.BlockCount);
	for (std::uint32_t Index = 0; Index < Prefix.BlockCount; ++Index)
	{
		const auto Offset = Scalar<std::uint64_t>();
		const auto Size = Scalar<std::uint64_t>();
		if (Offset != Next || Size > Total - Next)
		{
			throw std::runtime_error("Invalid archive bulk range");
		}
		Blocks.emplace_back(Next, static_cast<std::size_t>(Size));
		Next += static_cast<std::size_t>(Size);
	}
	if (Next != Total)
	{
		throw std::runtime_error("Trailing archive bulk data");
	}
	End = MetadataEnd;
}

FArchiveNode FArchiveReader::Bulk()
{
	FBulkData Data;
	const auto Element = ParseBulkElement(String());
	if (!Element)
	{
		throw std::runtime_error("Invalid bulk element type");
	}
	Data.Element = *Element;
	std::size_t Offset{};
	std::size_t Size{};
	if (bLegacy)
	{
		Size = Scalar<std::uint32_t>();
		Offset = Position;
		(void)Raw(Size);
	}
	else
	{
		const auto Index = Scalar<std::uint32_t>();
		if (Index != BulkReads++ || Index >= Blocks.size())
		{
			throw std::runtime_error("Invalid archive bulk index");
		}
		Offset = Blocks[Index].first;
		Size = Blocks[Index].second;
	}
	const auto ElementBytes = GetBulkElementInfo(Data.Element).ByteSize;
	if (Size % ElementBytes)
	{
		throw std::runtime_error("Invalid bulk element alignment");
	}
	// Account for final typed storage as well as an intermediate copy when no owner is supplied.
	if (bMetadataOnly)
	{
		return FArchiveNode(std::monostate{});
	}
	Charge(Size);
	if (Storage)
	{
		Data.Storage = Storage;
		Data.Offset = StorageOffset + Offset;
		Data.Size = Size;
	}
	else
	{
		Charge(Size);
		const auto View = Bytes.subspan(Offset, Size);
		Data.Bytes.assign(View.begin(), View.end());
	}
	return FArchiveNode(std::move(Data));
}

FArchiveNode FArchiveReader::Container(EArchiveTag InTag, unsigned InDepth)
{
	const auto Count = Scalar<std::uint32_t>();
	if (Count > Limits.MaxNodes - Nodes || Count > End - Position)
	{
		throw std::runtime_error("Invalid archive container count");
	}
	FArchiveNode::FArray Array;
	FArchiveNode::FObject Object;
	if (InTag == EArchiveTag::Array)
	{
		Charge(std::size_t(Count) * sizeof(FArchiveNode));
		Array.reserve(Count);
	}
	for (std::uint32_t Index = 0; Index < Count; ++Index)
	{
		if (InTag == EArchiveTag::Array)
		{
			Array.push_back(Node(InDepth + 1));
		}
		else
		{
			auto Key = String();
			auto Value = Node(InDepth + 1);
			if (!Object.emplace(std::move(Key), std::move(Value)).second)
			{
				throw std::runtime_error("Duplicate archive key");
			}
		}
	}
	return InTag == EArchiveTag::Array ? FArchiveNode(std::move(Array)) : FArchiveNode(std::move(Object));
}

FArchiveNode FArchiveReader::Node(unsigned InDepth)
{
	if (InDepth > Limits.MaxDepth || ++Nodes > Limits.MaxNodes)
	{
		throw std::runtime_error("Archive complexity limit exceeded");
	}
	Charge(256); // Conservative node/map overhead; separate charges account for variable-sized storage.
	const auto Tag = static_cast<EArchiveTag>(Scalar<std::uint8_t>());
	if (bLegacy && Tag > EArchiveTag::Object)
	{
		throw std::runtime_error("Invalid legacy archive tag");
	}
	switch (Tag)
	{
		case EArchiveTag::Boolean:
		{
			const auto Value = Scalar<std::uint8_t>();
			if (Value > 1)
			{
				throw std::runtime_error("Invalid archive boolean");
			}
			return FArchiveNode(Value != 0);
		}
		case EArchiveTag::Signed:
			return FArchiveNode(Scalar<std::int64_t>());
		case EArchiveTag::Unsigned:
			return FArchiveNode(Scalar<std::uint64_t>());
		case EArchiveTag::Number:
		{
			const auto Value = Scalar<double>();
			if (!std::isfinite(Value))
			{
				throw std::runtime_error("Non-finite archive number");
			}
			return FArchiveNode(Value);
		}
		case EArchiveTag::String:
			return FArchiveNode(String());
		case EArchiveTag::Bulk:
			return Bulk();
		case EArchiveTag::Array:
		case EArchiveTag::Object:
			return Container(Tag, InDepth);
		case EArchiveTag::Null:
			return FArchiveNode(std::monostate{});
		default:
			throw std::runtime_error("Invalid archive tag");
	}
}
} // namespace Hyperion::Private

namespace Hyperion
{
std::size_t GetArchiveMetadataPrefixSize()
{
	return Private::ArchivePrefixSize;
}

std::size_t ProbeArchiveMetadataSize(std::span<const std::byte> InPrefix, std::size_t InTotalBytes,
                                     FArchiveLimits InLimits)
{
	return Private::ResolveArchiveMetadataSize(Private::ReadArchivePrefix(InPrefix, InLimits), InTotalBytes, InLimits);
}
} // namespace Hyperion
