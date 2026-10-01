#include "Hyperion/Materials/ShaderParameterLayout.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace Hyperion
{
namespace
{
std::uint32_t CheckedAdd(std::uint32_t InLeft, std::uint32_t InRight, const std::string& InName)
{
	if (InRight > std::numeric_limits<std::uint32_t>::max() - InLeft)
	{
		throw std::invalid_argument("Shader layout overflow: " + InName);
	}
	return InLeft + InRight;
}

std::uint32_t AlignRegister(std::uint32_t InOffset, const std::string& InName)
{
	return CheckedAdd(InOffset, 15, InName) & ~std::uint32_t{15};
}
} // namespace

void ValidateShaderTextureType(const FMaterialParameterType& InType, std::string_view InName)
{
	InType.Validate();
	if (InType.Kind != EMaterialValueKind::Numeric || InType.Scalar != EMaterialScalar::Float || InType.Rows != 1 ||
	    InType.ArrayCount != 0)
	{
		throw std::invalid_argument("Unsupported shader texture element: " + std::string(InName));
	}
}

FShaderParameterLayoutBuilder::FShaderParameterLayoutBuilder(std::string InName, std::string InInstance,
                                                             EShaderPackingProfile InProfile)
    : Name(std::move(InName)), Profile(InProfile)
{
	if (Name.empty() || (Profile != EShaderPackingProfile::Uniform && Profile != EShaderPackingProfile::Structured))
	{
		throw std::invalid_argument("Invalid shader packing profile: " + Name);
	}
	Block.Instance = std::move(InInstance);
}

void FShaderParameterLayoutBuilder::AddField(std::string InName, const FMaterialParameterType& InType,
                                             FMaterialSemanticId InSemantic)
{
	InType.Validate();
	const bool bMatrix = InType.Rows > 1;
	if ((Profile == EShaderPackingProfile::Uniform && InName.empty()) || InType.Kind != EMaterialValueKind::Numeric ||
	    InType.ArrayCount != 0 ||
	    (bMatrix && (Profile != EShaderPackingProfile::Uniform || InType.Scalar != EMaterialScalar::Float ||
	                 InType.Rows != 4 || InType.Columns != 4)))
	{
		throw std::invalid_argument("Unsupported shader packing type: " + Name + "." + InName);
	}
	const std::uint32_t Size = InType.Columns * (bMatrix ? 16 : 4);
	std::uint32_t Offset = Cursor;
	if (Profile == EShaderPackingProfile::Uniform && (bMatrix || Offset % 16 + Size > 16))
	{
		Offset = AlignRegister(Offset, Name);
	}
	if (bExplicitOffset && Offset != Cursor)
	{
		throw std::invalid_argument("Misaligned fixed shader offset: " + Name + "." + InName);
	}
	const std::string Field = Block.Instance.empty() ? std::move(InName) : Block.Instance + "." + InName;
	if (std::any_of(Block.Members.begin(), Block.Members.end(),
	                [&](const auto& InField)
	                {
		                return InField.Name == Field;
	                }))
	{
		throw std::invalid_argument("Duplicate shader field: " + Name + "." + Field);
	}
	Cursor = CheckedAdd(Offset, Size, Name);
	bExplicitOffset = false;
	Block.Members.push_back({Field, std::move(InSemantic), Offset, InType.Columns, InType.Rows, InType.Scalar});
}

void FShaderParameterLayoutBuilder::SetNextOffset(std::uint32_t InOffset)
{
	if (InOffset < Cursor || InOffset % 4 != 0 || bExplicitOffset)
	{
		throw std::invalid_argument("Invalid fixed shader offset: " + Name);
	}
	Cursor = InOffset;
	bExplicitOffset = true;
}

void FShaderParameterLayoutBuilder::SetMinimumSize(std::uint32_t InSize)
{
	if (MinimumSize != 0 || InSize == 0 || InSize % (Profile == EShaderPackingProfile::Uniform ? 16 : 4) != 0)
	{
		throw std::invalid_argument("Invalid fixed shader extent: " + Name);
	}
	MinimumSize = InSize;
}

FStandardMaterialBlock FShaderParameterLayoutBuilder::Build() const
{
	if (Block.Members.empty() || bExplicitOffset || (MinimumSize != 0 && MinimumSize < Cursor))
	{
		throw std::invalid_argument("Incomplete fixed shader layout: " + Name);
	}
	FStandardMaterialBlock Result = Block;
	Result.Size =
	    std::max(MinimumSize, Profile == EShaderPackingProfile::Uniform ? AlignRegister(Cursor, Name) : Cursor);
	return Result;
}
} // namespace Hyperion
