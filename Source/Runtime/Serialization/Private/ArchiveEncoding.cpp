#include "ArchiveInternal.h"
#include "Hyperion/Core/ContentHash.h"

namespace Hyperion
{
namespace
{
Private::FArchiveWriter Prefix(const FArchiveNode& InNode, FArchiveLimits InLimits)
{
	Private::FArchiveWriter Metadata{InLimits};
	Metadata.Node(InNode, 0);
	if (InLimits.MaxBytes < 24 || Metadata.Blocks.size() > (InLimits.MaxBytes - 24) / 16)
	{
		throw std::runtime_error("Archive block directory limit exceeded");
	}
	const auto PrefixSize = 24 + Metadata.Blocks.size() * 16;
	if (Metadata.Bytes.size() > InLimits.MaxBytes - PrefixSize)
	{
		throw std::runtime_error("Archive size limit exceeded");
	}
	auto Size = PrefixSize + Metadata.Bytes.size();
	for (const auto Block : Metadata.Blocks)
	{
		if (Block.size() > InLimits.MaxBytes - Size)
		{
			throw std::runtime_error("Archive bulk size limit exceeded");
		}
		Size += Block.size();
	}
	Private::FArchiveWriter Output{InLimits};
	Output.Bytes.reserve(PrefixSize + Metadata.Bytes.size());
	Output.Raw(Private::ArchiveMagic);
	Output.Scalar(std::uint32_t{2});
	Output.Scalar(static_cast<std::uint64_t>(Metadata.Bytes.size()));
	Output.Scalar(static_cast<std::uint32_t>(Metadata.Blocks.size()));
	Output.Scalar(std::uint32_t{0});
	auto Offset = PrefixSize + Metadata.Bytes.size();
	for (const auto Block : Metadata.Blocks)
	{
		Output.Scalar(static_cast<std::uint64_t>(Offset));
		Output.Scalar(static_cast<std::uint64_t>(Block.size()));
		Offset += Block.size();
	}
	Output.Raw(Metadata.Bytes);
	Output.Blocks = std::move(Metadata.Blocks);
	return Output;
}
} // namespace

std::vector<std::byte> EncodeArchive(const FArchiveNode& InNode, FArchiveLimits InLimits)
{
	auto Output = Prefix(InNode, InLimits);
	auto Size = Output.Bytes.size();
	for (const auto Block : Output.Blocks)
	{
		Size += Block.size();
	}
	Output.Bytes.reserve(Size);
	for (const auto Block : Output.Blocks)
	{
		Output.Raw(Block);
	}
	return std::move(Output.Bytes);
}

std::string HashArchive(const FArchiveNode& InNode, FArchiveLimits InLimits)
{
	auto Output = Prefix(InNode, InLimits);
	std::vector<std::span<const std::byte>> Parts;
	Parts.reserve(Output.Blocks.size() + 1);
	Parts.emplace_back(Output.Bytes);
	Parts.insert(Parts.end(), Output.Blocks.begin(), Output.Blocks.end());
	return ContentHashParts(Parts);
}
} // namespace Hyperion
