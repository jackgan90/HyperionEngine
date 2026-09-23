#pragma once
#include "Hyperion/Reflection/Wire.h"

namespace Hyperion::WirePrivate
{
FArchiveNode Encode(const FRecordValueShape& InShape, const FArchiveNode& InValue, unsigned InDepth = 0);
FArchiveNode Decode(const FRecordValueShape& InShape, const FArchiveNode& InValue, const std::string& InPath,
                    unsigned InDepth = 0);
FArchiveNode Schema(const FRecordValueShape& InShape, unsigned InDepth = 0);
FArchiveNode::FArray Unpack(const FBulkData& InBulk);
FBulkData Pack(const FRecordValueShape& InShape, const FArchiveNode::FArray& InValues);
bool IsBulk(const FRecordValueShape& InShape);
void CheckDepth(unsigned InDepth);
} // namespace Hyperion::WirePrivate
