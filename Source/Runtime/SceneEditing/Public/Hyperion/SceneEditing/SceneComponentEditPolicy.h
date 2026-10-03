#pragma once
#include "Hyperion/Reflection/Record.h"

namespace Hyperion
{
// Field IDs are resolved against the canonical component descriptor, never display labels.
bool IsSceneComponentFieldReadOnly(const FRecordDescriptor& InType, std::string_view InField);
// Checks component-edit admission only; generic document/history restoration has separate semantics.
void ValidateSceneComponentEdit(const FRecordDescriptor& InType, const void* InOriginal, const void* InCandidate);
} // namespace Hyperion
