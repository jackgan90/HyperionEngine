#include "Hyperion/Renderer/MaterialPreparation.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
FShaderBinding PrepareMaterialInstanceBinding(const FShaderBinding& InResource, const FMaterialPass& InPass,
                                              FMaterialProgramBinding& OutBinding)
{
	OutBinding.Resource = InResource;
	const auto Declaration = std::find_if(InPass.InstanceArrays.begin(), InPass.InstanceArrays.end(),
	                                      [&](const auto& InArray)
	                                      {
		                                      return InArray.Block == InResource.Name;
	                                      });
	if (Declaration == InPass.InstanceArrays.end())
	{
		return InResource;
	}
	if (InResource.Kind != EBindingKind::UniformBuffer || InResource.Count != 1 || InResource.Members.size() != 1)
	{
		throw std::invalid_argument("Instance block must contain exactly one constant array: " + InResource.Name);
	}
	const auto& Array = InResource.Members.front();
	if (Array.Name != Declaration->Member || Array.Kind != EShaderValueKind::Array || Array.Offset != 0 ||
	    Array.ArrayCount == 0 || Array.ArrayStride == 0 || Array.ArrayStride % 16 != 0 || Array.Members.size() != 1 ||
	    Array.Members.front().Kind != EShaderValueKind::Structure || Array.Members.front().Offset != 0 ||
	    std::uint64_t(Array.ArrayStride) * Array.ArrayCount > 65536 ||
	    InResource.ByteSize > std::uint64_t(Array.ArrayStride) * Array.ArrayCount)
	{
		throw std::invalid_argument("Invalid reflected material instance array: " + InResource.Name);
	}
	OutBinding.InstanceStride = Array.ArrayStride;
	OutBinding.InstanceCapacity = Array.ArrayCount;
	OutBinding.Resource.ByteSize = Array.ArrayStride * Array.ArrayCount;
	FShaderBinding Record = InResource;
	Record.ByteSize = Array.ArrayStride;
	Record.Members = Array.Members.front().Members;
	return Record;
}
} // namespace Hyperion
