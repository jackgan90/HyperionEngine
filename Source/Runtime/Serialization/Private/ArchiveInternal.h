#pragma once
#include "Hyperion/Serialization/Archive.h"
#include <array>

namespace Hyperion::Private
{
enum class EArchiveTag : std::uint8_t
{
	Boolean = 0,
	Signed = 1,
	Number = 2,
	String = 3,
	Bulk = 4,
	Array = 5,
	Object = 6,
	Null = 7,
	Unsigned = 8
};

constexpr std::array<std::byte, 4> ArchiveMagic{std::byte{'H'}, std::byte{'Y'}, std::byte{'P'}, std::byte{'A'}};

struct FArchiveWriter
{
	FArchiveLimits Limits;
	std::vector<std::byte> Bytes;
	std::vector<std::span<const std::byte>> Blocks;
	std::size_t Nodes{};

	void Raw(std::span<const std::byte> InData);
	void String(const std::string& InValue);
	void Node(const FArchiveNode& InNode, unsigned InDepth);
	void Bulk(const FBulkData& InData);

	template<class T> void Scalar(T InValue)
	{
		if constexpr (std::is_same_v<T, double>)
		{
			Scalar(std::bit_cast<std::uint64_t>(InValue));
		}
		else
		{
			using FUnsigned = std::make_unsigned_t<T>;
			auto Value = std::bit_cast<FUnsigned>(InValue);
			std::array<std::byte, sizeof(T)> Data{};
			for (auto& Byte : Data)
			{
				Byte = std::byte(Value & 255);
				Value >>= 8;
			}
			Raw(Data);
		}
	}

	void Tag(EArchiveTag InTag)
	{
		Scalar(static_cast<std::uint8_t>(InTag));
	}
};

struct FArchiveReader
{
	FArchiveLimits Limits;
	std::span<const std::byte> Bytes;
	std::shared_ptr<const std::vector<std::byte>> Storage;
	std::size_t StorageOffset{};
	std::size_t Position{};
	std::size_t End{};
	std::size_t Nodes{};
	std::size_t Allocated{};
	bool bLegacy{};
	std::vector<std::pair<std::size_t, std::size_t>> Blocks;
	std::size_t BulkReads{};

	void Charge(std::size_t InBytes);
	std::span<const std::byte> Raw(std::size_t InSize);
	std::string String();
	FArchiveNode Node(unsigned InDepth);
	FArchiveNode Container(EArchiveTag InTag, unsigned InDepth);
	FArchiveNode Bulk();
	void Header();

	template<class T> T Scalar()
	{
		if constexpr (std::is_same_v<T, double>)
		{
			return std::bit_cast<double>(Scalar<std::uint64_t>());
		}
		else
		{
			using FUnsigned = std::make_unsigned_t<T>;
			const auto Data = Raw(sizeof(T));
			FUnsigned Value{};
			for (std::size_t Index = 0; Index < Data.size(); ++Index)
			{
				Value |= static_cast<FUnsigned>(std::to_integer<unsigned>(Data[Index])) << (Index * 8);
			}
			return std::bit_cast<T>(Value);
		}
	}
};
} // namespace Hyperion::Private
