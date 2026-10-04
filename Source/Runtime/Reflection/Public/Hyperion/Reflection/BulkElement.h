#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Hyperion
{
// The only supported-element list. Enum ordinals are internal, never archive values.
#define HYP_BULK_ELEMENTS(X)                                                                                           \
	X(U8, std::uint8_t, "u8")                                                                                          \
	X(I8, std::int8_t, "i8")                                                                                           \
	X(U16, std::uint16_t, "u16")                                                                                       \
	X(I16, std::int16_t, "i16")                                                                                        \
	X(U32, std::uint32_t, "u32")                                                                                       \
	X(I32, std::int32_t, "i32")                                                                                        \
	X(U64, std::uint64_t, "u64")                                                                                       \
	X(I64, std::int64_t, "i64")                                                                                        \
	X(F32, float, "f32")                                                                                               \
	X(F64, double, "f64")

enum class EBulkElement : std::uint8_t
{
	Invalid,
#define HYP_BULK_ENUM(Name, Type, WireName) Name,
	HYP_BULK_ELEMENTS(HYP_BULK_ENUM)
#undef HYP_BULK_ENUM
};

enum class EBulkElementKind
{
	UnsignedInteger,
	SignedInteger,
	FloatingPoint
};

template<class T> constexpr EBulkElementKind BulkElementKind()
{
	return std::is_floating_point_v<T> ? EBulkElementKind::FloatingPoint
	       : std::is_signed_v<T>       ? EBulkElementKind::SignedInteger
	                                   : EBulkElementKind::UnsignedInteger;
}

struct FBulkElementInfo
{
	EBulkElement Element;
	std::string_view WireName;
	std::size_t ByteSize;
	EBulkElementKind Kind;
};

inline constexpr std::array BulkElements{
#define HYP_BULK_INFO(Name, Type, WireName)                                                                            \
	FBulkElementInfo{EBulkElement::Name, WireName, sizeof(Type), BulkElementKind<Type>()},
    HYP_BULK_ELEMENTS(HYP_BULK_INFO)
#undef HYP_BULK_INFO
};

constexpr const FBulkElementInfo* FindBulkElementInfo(EBulkElement InElement)
{
	for (const auto& Info : BulkElements)
	{
		if (Info.Element == InElement)
		{
			return &Info;
		}
	}
	return nullptr;
}

constexpr const FBulkElementInfo& GetBulkElementInfo(EBulkElement InElement)
{
	if (const auto* Info = FindBulkElementInfo(InElement))
	{
		return *Info;
	}
	throw std::invalid_argument("Unsupported bulk element");
}

constexpr std::optional<EBulkElement> ParseBulkElement(std::string_view InName)
{
	for (const auto& Info : BulkElements)
	{
		if (Info.WireName == InName)
		{
			return Info.Element;
		}
	}
	return std::nullopt;
}

template<class T> constexpr EBulkElement BulkElement()
{
	static_assert(std::is_arithmetic_v<T> || std::is_same_v<T, std::byte>);
	for (const auto& Info : BulkElements)
	{
		if (Info.Kind == BulkElementKind<T>() && Info.ByteSize == sizeof(T))
		{
			return Info.Element;
		}
	}
	throw std::invalid_argument("Unsupported C++ bulk element");
}

template<class F> decltype(auto) VisitBulkElement(EBulkElement InElement, F&& InVisitor)
{
	switch (InElement)
	{
#define HYP_BULK_VISIT(Name, Type, WireName)                                                                           \
	case EBulkElement::Name:                                                                                           \
		return std::forward<F>(InVisitor).template operator()<Type>();
		HYP_BULK_ELEMENTS(HYP_BULK_VISIT)
#undef HYP_BULK_VISIT
		default:
			throw std::invalid_argument("Unsupported bulk element");
	}
}

#undef HYP_BULK_ELEMENTS
} // namespace Hyperion
