#pragma once
#include "Hyperion/Materials/MaterialResources.h"
#include "Hyperion/RHI/RHIBindings.h"

namespace Hyperion
{
inline std::uint32_t GetMaterialBufferUsage(const FMaterialReadBufferSource& InSource)
{
	return ShaderReadBufferUsages | (InSource.IsStorage() ? ShaderWriteBufferUsages : 0U);
}
} // namespace Hyperion
