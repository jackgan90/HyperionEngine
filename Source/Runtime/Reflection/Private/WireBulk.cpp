#include "WireInternal.h"
#include <cstring>

namespace Hyperion::WirePrivate
{
namespace
{
template<class T> FArchiveNode::FArray UnpackTyped(const FBulkData& InBulk)
{
	const auto Bytes = InBulk.Data();
	if (Bytes.size() % sizeof(T))
	{
		throw std::invalid_argument("Misaligned archive bulk data");
	}
	FArchiveNode::FArray Result;
	Result.reserve(Bytes.size() / sizeof(T));
	for (std::size_t Offset = 0; Offset < Bytes.size(); Offset += sizeof(T))
	{
		T Value{};
		std::memcpy(&Value, Bytes.data() + Offset, sizeof(T));
		Result.push_back(WriteValue(Value));
	}
	return Result;
}

template<class T> FBulkData PackTyped(const FArchiveNode::FArray& InValues)
{
	std::vector<T> Values;
	Values.reserve(InValues.size());
	for (const auto& Value : InValues)
	{
		Values.push_back(ReadValue<T>(Value));
	}
	return std::get<FBulkData>(WriteBulk(Values).Value);
}

template<class T> bool Matches(const FRecordValueShape& InShape)
{
	const auto& Shape = RecordValueShape<T>();
	return InShape.Kind == Shape.Kind && InShape.ElementBytes == Shape.ElementBytes;
}
} // namespace

bool IsBulk(const FRecordValueShape& InShape)
{
	return InShape.bBulkSequence;
}

FArchiveNode::FArray Unpack(const FBulkData& InBulk)
{
#define HYP_UNPACK(Type)                                                                                               \
	if (InBulk.Element == BulkElement<Type>())                                                                         \
	{                                                                                                                  \
		return UnpackTyped<Type>(InBulk);                                                                              \
	}
	HYP_UNPACK(std::int8_t)
	HYP_UNPACK(std::uint8_t)
	HYP_UNPACK(std::int16_t)
	HYP_UNPACK(std::uint16_t)
	HYP_UNPACK(std::int32_t)
	HYP_UNPACK(std::uint32_t)
	HYP_UNPACK(std::int64_t)
	HYP_UNPACK(std::uint64_t)
	HYP_UNPACK(float)
	HYP_UNPACK(double)
#undef HYP_UNPACK
	throw std::invalid_argument("Unsupported archive bulk element: " + InBulk.Element);
}

FBulkData Pack(const FRecordValueShape& InShape, const FArchiveNode::FArray& InValues)
{
#define HYP_PACK(Type)                                                                                                 \
	if (Matches<Type>(InShape))                                                                                        \
	{                                                                                                                  \
		return PackTyped<Type>(InValues);                                                                              \
	}
	HYP_PACK(std::int8_t)
	HYP_PACK(std::uint8_t)
	HYP_PACK(std::int16_t)
	HYP_PACK(std::uint16_t)
	HYP_PACK(std::int32_t)
	HYP_PACK(std::uint32_t)
	HYP_PACK(std::int64_t)
	HYP_PACK(std::uint64_t)
	HYP_PACK(float)
	HYP_PACK(double)
#undef HYP_PACK
	throw std::invalid_argument("Unsupported wire bulk element");
}
} // namespace Hyperion::WirePrivate
