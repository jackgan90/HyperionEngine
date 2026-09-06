#include "Hyperion/Serialization/Archive.h"
#include <array>

namespace Hyperion
{
namespace
{
constexpr std::size_t MaxBytes = 512u * 1024u * 1024u;
constexpr std::array<std::byte, 8> Magic{std::byte{'H'}, std::byte{'Y'}, std::byte{'P'}, std::byte{'A'},
                                         std::byte{1},   std::byte{0},   std::byte{0},   std::byte{0}};

struct FWriter
{
	std::vector<std::byte> Bytes;

	void Raw(std::span<const std::byte> InData)
	{
		if (InData.size() > MaxBytes - Bytes.size())
		{
			throw std::runtime_error("Archive size limit exceeded");
		}
		Bytes.insert(Bytes.end(), InData.begin(), InData.end());
	}

	template<class T> void Scalar(T InValue)
	{
		Raw(std::as_bytes(std::span(&InValue, 1)));
	}

	void String(const std::string& InValue)
	{
		Scalar(static_cast<std::uint32_t>(InValue.size()));
		Raw(std::as_bytes(std::span(InValue)));
	}

	void Node(const FArchiveNode& InNode, unsigned InDepth)
	{
		if (InDepth > 64)
		{
			throw std::runtime_error("Archive nesting limit exceeded");
		}
		Scalar(static_cast<std::uint8_t>(InNode.Value.index()));
		std::visit(
		    [&](const auto& InValue)
		    {
			    using FType = std::decay_t<decltype(InValue)>;
			    if constexpr (std::is_same_v<FType, bool>)
			    {
				    Scalar(static_cast<std::uint8_t>(InValue));
			    }
			    else if constexpr (std::is_arithmetic_v<FType>)
			    {
				    Scalar(InValue);
			    }
			    else if constexpr (std::is_same_v<FType, std::string>)
			    {
				    String(InValue);
			    }
			    else if constexpr (std::is_same_v<FType, FBulkData>)
			    {
				    String(InValue.Element);
				    Scalar(static_cast<std::uint32_t>(InValue.Bytes.size()));
				    Raw(InValue.Bytes);
			    }
			    else
			    {
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
};

struct FReader
{
	std::span<const std::byte> Bytes;
	std::size_t Position{};
	std::size_t Nodes{};

	std::span<const std::byte> Raw(std::size_t InSize)
	{
		if (InSize > Bytes.size() - Position)
		{
			throw std::runtime_error("Truncated archive");
		}
		auto Result = Bytes.subspan(Position, InSize);
		Position += InSize;
		return Result;
	}

	template<class T> T Scalar()
	{
		T Value{};
		auto Data = Raw(sizeof(T));
		std::memcpy(&Value, Data.data(), sizeof(T));
		return Value;
	}

	std::string String()
	{
		const auto Data = Raw(Scalar<std::uint32_t>());
		return {reinterpret_cast<const char*>(Data.data()), Data.size()};
	}

	FArchiveNode Node(unsigned InDepth)
	{
		if (InDepth > 64 || ++Nodes > 1000000)
		{
			throw std::runtime_error("Archive complexity limit exceeded");
		}
		const auto Tag = Scalar<std::uint8_t>();
		switch (Tag)
		{
			case 0:
			{
				auto Value = Scalar<std::uint8_t>();
				if (Value > 1)
				{
					throw std::runtime_error("Invalid boolean");
				}
				return FArchiveNode(Value != 0);
			}
			case 1:
				return FArchiveNode(Scalar<std::int64_t>());
			case 2:
				return FArchiveNode(Scalar<double>());
			case 3:
				return FArchiveNode(String());
			case 4:
			{
				FBulkData Data;
				Data.Element = String();
				auto BytesView = Raw(Scalar<std::uint32_t>());
				Data.Bytes.assign(BytesView.begin(), BytesView.end());
				return FArchiveNode(std::move(Data));
			}
			case 5:
			case 6:
			{
				const auto Count = Scalar<std::uint32_t>();
				if (Count > 1000000 || Count > Bytes.size() - Position)
				{
					throw std::runtime_error("Invalid archive container count");
				}
				FArchiveNode::FArray Array;
				FArchiveNode::FObject Object;
				for (std::uint32_t Index = 0; Index < Count; ++Index)
				{
					if (Tag == 5)
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
				return Tag == 5 ? FArchiveNode(std::move(Array)) : FArchiveNode(std::move(Object));
			}
			default:
				throw std::runtime_error("Invalid archive tag");
		}
	}
};
} // namespace

std::vector<std::byte> EncodeArchive(const FArchiveNode& InNode)
{
	FWriter Writer;
	Writer.Raw(Magic);
	Writer.Node(InNode, 0);
	return std::move(Writer.Bytes);
}

FArchiveNode DecodeArchive(std::span<const std::byte> InBytes)
{
	if (InBytes.size() < Magic.size() || InBytes.size() > MaxBytes ||
	    !std::equal(Magic.begin(), Magic.end(), InBytes.begin()))
	{
		throw std::runtime_error("Invalid Hyperion archive header/size");
	}
	FReader Reader{InBytes, Magic.size()};
	auto Result = Reader.Node(0);
	if (Reader.Position != InBytes.size())
	{
		throw std::runtime_error("Trailing archive data");
	}
	return Result;
}
} // namespace Hyperion
