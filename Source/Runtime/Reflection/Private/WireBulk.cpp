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

ERecordValueKind RecordKind(EBulkElementKind InKind)
{
	switch (InKind)
	{
		case EBulkElementKind::SignedInteger:
			return ERecordValueKind::Integer;
		case EBulkElementKind::UnsignedInteger:
			return ERecordValueKind::UnsignedInteger;
		case EBulkElementKind::FloatingPoint:
			return ERecordValueKind::Number;
	}
	throw std::invalid_argument("Unsupported bulk numeric kind");
}
} // namespace

bool IsBulk(const FRecordValueShape& InShape)
{
	return InShape.bBulkSequence;
}

FArchiveNode::FArray Unpack(const FBulkData& InBulk)
{
	return VisitBulkElement(InBulk.Element,
	                        [&]<class T>()
	                        {
		                        return UnpackTyped<T>(InBulk);
	                        });
}

FBulkData Pack(const FRecordValueShape& InShape, const FArchiveNode::FArray& InValues)
{
	for (const auto& Info : BulkElements)
	{
		if (InShape.Kind == RecordKind(Info.Kind) && InShape.ElementBytes == Info.ByteSize)
		{
			return VisitBulkElement(Info.Element,
			                        [&]<class T>()
			                        {
				                        return PackTyped<T>(InValues);
			                        });
		}
	}
	throw std::invalid_argument("Unsupported wire bulk element");
}
} // namespace Hyperion::WirePrivate
