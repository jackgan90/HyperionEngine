#include "Hyperion/Renderer/MaterialBlocks.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
namespace
{

FShaderMember Layout(const FStandardMaterialBlockMember& InMember)
{
	FShaderMember Result;
	Result.Name = InMember.Name;
	Result.Offset = InMember.Offset;
	Result.Columns = InMember.Columns;
	Result.Rows = InMember.Rows;
	Result.Scalar = InMember.Scalar == EMaterialScalar::Uint   ? EShaderScalar::Uint
	                : InMember.Scalar == EMaterialScalar::Bool ? EShaderScalar::Bool
	                : InMember.Scalar == EMaterialScalar::Int  ? EShaderScalar::Int
	                                                           : EShaderScalar::Float;
	Result.MatrixStride = InMember.Rows > 1 ? 16 : 0;
	Result.Size = InMember.Rows > 1 ? InMember.Columns * 16 : InMember.Columns * 4;
	return Result;
}
} // namespace

bool NormalizeStandardMaterialBlock(FShaderBinding& InBinding)
{
	const FStandardMaterialBlock Block = GetStandardMaterialBlock(InBinding.Name);
	if (Block.Size == 0)
	{
		return false;
	}
	if (InBinding.Kind != EBindingKind::UniformBuffer || InBinding.Count != 1 || InBinding.ByteSize > Block.Size)
	{
		throw std::invalid_argument("Invalid standard material block extent: " + InBinding.Name);
	}
	for (const auto& Reflected : InBinding.Members)
	{
		if (!Reflected.bActive)
		{
			continue;
		}
		const auto Expected = std::find_if(Block.Members.begin(), Block.Members.end(),
		                                   [&](const FStandardMaterialBlockMember& InMember)
		                                   {
			                                   return InMember.Name == Reflected.Name;
		                                   });
		if (Expected == Block.Members.end())
		{
			throw std::invalid_argument("Unknown active standard block member: " + Reflected.Name);
		}
		const FShaderMember Required = Layout(*Expected);
		if (Reflected.Kind != Required.Kind || Reflected.Scalar != Required.Scalar ||
		    Reflected.Offset != Required.Offset || Reflected.Rows != Required.Rows ||
		    Reflected.Columns != Required.Columns || Reflected.MatrixStride != Required.MatrixStride ||
		    Reflected.bRowMajor != Required.bRowMajor)
		{
			throw std::invalid_argument("Standard material block ABI mismatch: " + InBinding.Name + "." +
			                            Reflected.Name);
		}
	}
	InBinding.ByteSize = Block.Size;
	InBinding.Members.clear();
	// A standard block is one complete input contract, including fields unused by a particular stage.
	for (const auto& Member : Block.Members)
	{
		InBinding.Members.push_back(Layout(Member));
	}
	return true;
}

} // namespace Hyperion
