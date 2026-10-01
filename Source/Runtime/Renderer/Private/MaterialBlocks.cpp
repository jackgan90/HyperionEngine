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

bool NormalizeStandardMaterialBlock(FShaderBinding& InBinding,
                                    std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	const FStandardMaterialBlock Block = GetStandardMaterialBlock(InBinding.Name, InContracts);
	if (Block.Size == 0)
	{
		if (InBinding.Name.starts_with("Hyperion") &&
		    GetEngineMaterialResource(InBinding.Name, InContracts).Semantic.IsEmpty())
		{
			throw std::invalid_argument("Unknown reserved engine resource: " + InBinding.Name);
		}
		return false;
	}
	if (InBinding.Kind != EBindingKind::UniformBuffer || InBinding.Count != 1 || InBinding.ByteSize > Block.Size)
	{
		throw std::invalid_argument("Invalid standard material block extent: " + InBinding.Name);
	}
	if (!Block.Instance.empty())
	{
		if (InBinding.Members.size() != 1 || InBinding.Members.front().Name != Block.Instance ||
		    InBinding.Members.front().Kind != EShaderValueKind::Structure || InBinding.Members.front().Offset != 0 ||
		    InBinding.Members.front().ArrayCount != 0)
		{
			throw std::invalid_argument("Invalid shader contract instance: " + InBinding.Name);
		}
		auto Members = std::move(InBinding.Members.front().Members);
		for (auto& Member : Members)
		{
			Member.Name = Block.Instance + "." + Member.Name;
		}
		InBinding.Members = std::move(Members);
	}
	for (const auto& Reflected : InBinding.Members)
	{
		const auto Expected = std::find_if(Block.Members.begin(), Block.Members.end(),
		                                   [&](const FStandardMaterialBlockMember& InMember)
		                                   {
			                                   return InMember.Name == Reflected.Name;
		                                   });
		if (Expected == Block.Members.end())
		{
			const bool bGeneratedPadding = Reflected.Name == (Block.Instance.empty() ? "" : Block.Instance + ".") +
			                                                     "HyperionPadding" + std::to_string(Reflected.Offset) &&
			                               Reflected.Scalar == EShaderScalar::Uint && Reflected.Columns == 1 &&
			                               Reflected.Size == 4;
			const bool bPadding = (!Reflected.bActive || bGeneratedPadding) &&
			                      Reflected.Kind == EShaderValueKind::Numeric && Reflected.Rows == 1 &&
			                      Reflected.Offset + Reflected.Size <= Block.Size &&
			                      std::none_of(Block.Members.begin(), Block.Members.end(),
			                                   [&](const auto& InMember)
			                                   {
				                                   const auto Required = Layout(InMember);
				                                   return Reflected.Offset < Required.Offset + Required.Size &&
				                                          Required.Offset < Reflected.Offset + Reflected.Size;
			                                   });
			if (bPadding)
			{
				continue;
			}
			throw std::invalid_argument("Unknown standard block member: " + InBinding.Name + "." + Reflected.Name);
		}
		const FShaderMember Required = Layout(*Expected);
		if (Reflected.Kind != Required.Kind || Reflected.Scalar != Required.Scalar ||
		    Reflected.Offset != Required.Offset || Reflected.Rows != Required.Rows ||
		    Reflected.Columns != Required.Columns || Reflected.MatrixStride != Required.MatrixStride ||
		    Reflected.bRowMajor != Required.bRowMajor || Reflected.ArrayCount != 0 || Reflected.ArrayStride != 0 ||
		    Reflected.Size != Required.Size || Reflected.Offset + Reflected.Size > InBinding.ByteSize)
		{
			throw std::invalid_argument("Standard material block ABI mismatch: " + InBinding.Name + "." +
			                            Reflected.Name);
		}
	}
	// Keep native offsets and extent. Only declared fields become inputs, including inactive contract fields.
	std::erase_if(InBinding.Members,
	              [&](const FShaderMember& InMember)
	              {
		              return std::none_of(Block.Members.begin(), Block.Members.end(),
		                                  [&](const auto& InExpected)
		                                  {
			                                  return InExpected.Name == InMember.Name;
		                                  });
	              });
	for (auto& Member : InBinding.Members)
	{
		Member.bActive = true;
	}
	return true;
}

FEngineMaterialResource ValidateEngineMaterialResource(
    const FShaderBinding& InBinding, std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	const auto Contract = GetEngineMaterialResource(InBinding.Name, InContracts);
	if (Contract.Semantic.IsEmpty())
	{
		return Contract;
	}
	const auto Fail = [&]
	{
		throw std::invalid_argument("Engine resource contract mismatch: " + InBinding.Name);
	};
	if (InBinding.Count != 1)
	{
		Fail();
	}
	if (Contract.Kind == EMaterialValueKind::Sampler)
	{
		if (InBinding.Kind != EBindingKind::Sampler || InBinding.bComparison != Contract.bComparison)
		{
			Fail();
		}
	}
	else if (Contract.Kind == EMaterialValueKind::ReadBuffer)
	{
		if (InBinding.Kind !=
		        (Contract.bWritable ? EBindingKind::StorageStructuredBuffer : EBindingKind::StructuredBuffer) ||
		    InBinding.StructureByteStride != Contract.StructureByteStride || InBinding.Members.size() != 1)
		{
			Fail();
		}
		const auto& Element = InBinding.Members.front();
		const auto& Members = Element.Kind == EShaderValueKind::Structure ? Element.Members : InBinding.Members;
		if (Members.size() != Contract.ElementLayout.Members.size())
		{
			Fail();
		}
		for (std::size_t Index = 0; Index < Members.size(); ++Index)
		{
			const auto& Reflected = Members[Index];
			const auto& Expected = Contract.ElementLayout.Members[Index];
			const auto Required = Layout(Expected);
			if (Reflected.Kind != Required.Kind || Reflected.Scalar != Required.Scalar ||
			    Reflected.Offset != Required.Offset || Reflected.Columns != Required.Columns ||
			    Reflected.Rows != Required.Rows || Reflected.Size != Required.Size ||
			    Reflected.MatrixStride != Required.MatrixStride || Reflected.bRowMajor != Required.bRowMajor ||
			    Reflected.ArrayCount != 0 || Reflected.ArrayStride != 0 ||
			    (!Expected.Name.empty() && Reflected.Name != Expected.Name))
			{
				Fail();
			}
		}
	}
	else
	{
		const auto Dimension = Contract.Kind == EMaterialValueKind::TextureCube ? EShaderResourceDimension::TextureCube
		                                                                        : EShaderResourceDimension::Texture2D;
		if (InBinding.Kind != (Contract.bWritable ? EBindingKind::StorageTexture : EBindingKind::Texture) ||
		    InBinding.Dimension != Dimension || InBinding.ResourceScalar != EShaderScalar::Float)
		{
			Fail();
		}
	}
	return Contract;
}

} // namespace Hyperion
