#include "ArchiveInternal.h"

namespace Hyperion::Private
{
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
	if (Bytes.size() < 8 || Bytes.size() > Limits.MaxBytes ||
	    !std::equal(ArchiveMagic.begin(), ArchiveMagic.end(), Bytes.begin()))
	{
		throw std::runtime_error("Invalid Hyperion archive header/size");
	}
	Position = 4;
	const auto Version = Scalar<std::uint32_t>();
	if (Version == 1)
	{
		bLegacy = true;
		return;
	}
	if (Version != 2)
	{
		throw std::runtime_error("Unsupported archive container version");
	}
	const auto MetadataSize = Scalar<std::uint64_t>();
	const auto Count = Scalar<std::uint32_t>();
	if (Scalar<std::uint32_t>() != 0 || Count > Limits.MaxNodes || Count > (Bytes.size() - Position) / 16)
	{
		throw std::runtime_error("Invalid archive block directory");
	}
	Charge(std::size_t(Count) * sizeof(Blocks.front()));
	const auto Prefix = Position + std::size_t(Count) * 16;
	if (MetadataSize > Bytes.size() - Prefix)
	{
		throw std::runtime_error("Invalid archive metadata length");
	}
	const auto MetadataEnd = Prefix + static_cast<std::size_t>(MetadataSize);
	auto Next = MetadataEnd;
	Blocks.reserve(Count);
	for (std::uint32_t Index = 0; Index < Count; ++Index)
	{
		const auto Offset = Scalar<std::uint64_t>();
		const auto Size = Scalar<std::uint64_t>();
		if (Offset != Next || Size > Bytes.size() - Next)
		{
			throw std::runtime_error("Invalid archive bulk range");
		}
		Blocks.emplace_back(Next, static_cast<std::size_t>(Size));
		Next += static_cast<std::size_t>(Size);
	}
	if (Next != Bytes.size())
	{
		throw std::runtime_error("Trailing archive bulk data");
	}
	End = MetadataEnd;
}

FArchiveNode FArchiveReader::Bulk()
{
	FBulkData Data;
	Data.Element = String();
	const std::array<std::string_view, 10> Elements{"u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f32", "f64"};
	if (std::find(Elements.begin(), Elements.end(), Data.Element) == Elements.end())
	{
		throw std::runtime_error("Invalid bulk element type");
	}
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
	const auto ElementBytes = static_cast<std::size_t>(std::stoul(Data.Element.substr(1))) / 8;
	if (Size % ElementBytes)
	{
		throw std::runtime_error("Invalid bulk element alignment");
	}
	// Account for final typed storage as well as an intermediate copy when no owner is supplied.
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
