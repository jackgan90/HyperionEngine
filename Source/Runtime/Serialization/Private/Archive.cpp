#include "ArchiveInternal.h"

namespace Hyperion::Private
{
void FArchiveWriter::Raw(std::span<const std::byte> InData)
{
	if (InData.size() > Limits.MaxBytes - Bytes.size())
	{
		throw std::runtime_error("Archive size limit exceeded");
	}
	Bytes.insert(Bytes.end(), InData.begin(), InData.end());
}

void FArchiveWriter::String(const std::string& InValue)
{
	if (InValue.size() > std::numeric_limits<std::uint32_t>::max())
	{
		throw std::runtime_error("Archive string size exceeds wire limit");
	}
	Scalar(static_cast<std::uint32_t>(InValue.size()));
	Raw(std::as_bytes(std::span(InValue)));
}

void FArchiveWriter::Bulk(const FBulkData& InData)
{
	Tag(EArchiveTag::Bulk);
	String(InData.Element);
	Scalar(static_cast<std::uint32_t>(Blocks.size()));
	Blocks.push_back(InData.Data());
}

void FArchiveWriter::Node(const FArchiveNode& InNode, unsigned InDepth)
{
	if (InDepth > Limits.MaxDepth || ++Nodes > Limits.MaxNodes)
	{
		throw std::runtime_error("Archive complexity limit exceeded");
	}
	std::visit(
	    [&](const auto& InValue)
	    {
		    using FType = std::decay_t<decltype(InValue)>;
		    if constexpr (std::is_same_v<FType, bool>)
		    {
			    Tag(EArchiveTag::Boolean);
			    Scalar(static_cast<std::uint8_t>(InValue));
		    }
		    else if constexpr (std::is_same_v<FType, std::int64_t>)
		    {
			    Tag(EArchiveTag::Signed);
			    Scalar(InValue);
		    }
		    else if constexpr (std::is_same_v<FType, std::uint64_t>)
		    {
			    Tag(EArchiveTag::Unsigned);
			    Scalar(InValue);
		    }
		    else if constexpr (std::is_same_v<FType, double>)
		    {
			    if (!std::isfinite(InValue))
			    {
				    throw std::runtime_error("Non-finite archive number");
			    }
			    Tag(EArchiveTag::Number);
			    Scalar(InValue);
		    }
		    else if constexpr (std::is_same_v<FType, std::string>)
		    {
			    Tag(EArchiveTag::String);
			    String(InValue);
		    }
		    else if constexpr (std::is_same_v<FType, FBulkData>)
		    {
			    Bulk(InValue);
		    }
		    else if constexpr (std::is_same_v<FType, std::monostate>)
		    {
			    Tag(EArchiveTag::Null);
		    }
		    else
		    {
			    if (InValue.size() > Limits.MaxNodes || InValue.size() > std::numeric_limits<std::uint32_t>::max())
			    {
				    throw std::runtime_error("Archive container count exceeded");
			    }
			    Tag(std::is_same_v<FType, FArchiveNode::FObject> ? EArchiveTag::Object : EArchiveTag::Array);
			    Scalar(static_cast<std::uint32_t>(InValue.size()));
			    for (const auto& Item : InValue)
			    {
				    if constexpr (std::is_same_v<FType, FArchiveNode::FObject>)
				    {
					    String(Item.first);
					    Node(Item.second, InDepth + 1);
				    }
				    else
				    {
					    Node(Item, InDepth + 1);
				    }
			    }
		    }
	    },
	    InNode.Value);
}
} // namespace Hyperion::Private

namespace Hyperion
{
namespace
{
FArchiveNode Decode(Private::FArchiveReader InReader)
{
	InReader.Header();
	auto Result = InReader.Node(0);
	if (InReader.Position != InReader.End || (!InReader.bLegacy && InReader.BulkReads != InReader.Blocks.size()))
	{
		throw std::runtime_error("Trailing archive data or unused bulk blocks");
	}
	return Result;
}
} // namespace

FArchiveNode DecodeArchive(std::span<const std::byte> InBytes, FArchiveLimits InLimits)
{
	return Decode({InLimits, InBytes, {}, 0, 0, InBytes.size()});
}

FArchiveNode DecodeArchive(std::shared_ptr<const std::vector<std::byte>> InBytes, std::size_t InOffset,
                           FArchiveLimits InLimits)
{
	if (!InBytes || InOffset > InBytes->size())
	{
		throw std::runtime_error("Invalid owned archive input");
	}
	const auto Bytes = std::span(*InBytes).subspan(InOffset);
	return Decode({InLimits, Bytes, std::move(InBytes), InOffset, 0, Bytes.size()});
}
} // namespace Hyperion
