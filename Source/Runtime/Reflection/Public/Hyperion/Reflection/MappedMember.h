#pragma once
#include "Hyperion/Reflection/RecordValue.h"

namespace Hyperion
{
// Keep a stable archive/wire shape while the owning domain stores a stronger semantic type.
template<class W, class T, class M, class TEncode, class TDecode>
FRecordMember MappedMember(std::string InId, M T::* InMember, TEncode InEncode, TDecode InDecode,
                           FRecordMemberOptions InOptions = {})
{
	return {std::move(InId),
	        [InMember, InEncode](const void* InObject)
	        {
		        return WriteValue(W(InEncode(static_cast<const T*>(InObject)->*InMember)));
	        },
	        [InMember, InDecode](void* InObject, const FArchiveNode& InNode, const FRecordReadContext& InContext)
	        {
		        static_cast<T*>(InObject)->*InMember = InDecode(ReadValue<W>(InNode, InContext));
	        },
	        [InMember, InEncode](const void* InObject, const FRecordVisitor& InVisitor, std::string_view InPath)
	        {
		        VisitValue(W(InEncode(static_cast<const T*>(InObject)->*InMember)), InVisitor, InPath);
	        },
	        std::move(InOptions),
	        &RecordValueShape<W>,
	        FRecordMemberAssociation(InMember)};
}
} // namespace Hyperion
