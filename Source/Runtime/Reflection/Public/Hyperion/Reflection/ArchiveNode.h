#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace Hyperion
{
struct FBulkData
{
	std::string Element;
	std::vector<std::byte> Bytes;
	std::shared_ptr<const std::vector<std::byte>> Storage;
	std::size_t Offset{};
	std::size_t Size{};

	std::span<const std::byte> Data() const
	{
		if (Storage && (Offset > Storage->size() || Size > Storage->size() - Offset))
		{
			throw std::runtime_error("Invalid archive bulk view");
		}
		return Storage ? std::span(*Storage).subspan(Offset, Size) : std::span(Bytes);
	}
};

struct FArchiveNode
{
	using FArray = std::vector<FArchiveNode>;
	using FObject = std::map<std::string, FArchiveNode>;
	std::variant<bool, std::int64_t, double, std::string, FBulkData, FArray, FObject, std::monostate, std::uint64_t>
	    Value;
	FArchiveNode() = default;

	template<class T> explicit FArchiveNode(T InValue) : Value(std::move(InValue))
	{
	}
};
} // namespace Hyperion
