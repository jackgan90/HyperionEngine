#include "Hyperion/Renderer/MaterialPacking.h"
#include "Hyperion/Core/Profiling.h"
#include <cstring>
#include <stdexcept>

namespace Hyperion
{
namespace
{
void WriteWord(std::span<std::byte> OutBytes, std::uint64_t InOffset, std::uint32_t InWord)
{
	if (InOffset > OutBytes.size() || sizeof(InWord) > OutBytes.size() - InOffset)
	{
		throw std::invalid_argument("Reflected material value exceeds constant buffer extent");
	}
	std::memcpy(OutBytes.data() + InOffset, &InWord, sizeof(InWord));
}

void PackValue(std::span<std::byte> OutBytes, const FShaderMember& InLayout, const FMaterialValue& InValue,
               std::uint64_t InBase, std::uint32_t InDepth = 0)
{
	if (InDepth > 32 || InLayout.Offset > OutBytes.size() || InBase > OutBytes.size() - InLayout.Offset)
	{
		throw std::invalid_argument("Invalid reflected material nesting or offset");
	}
	const std::uint64_t Base = InBase + InLayout.Offset;
	if (InLayout.Kind == EShaderValueKind::Structure)
	{
		for (std::size_t Index = 0; Index < InLayout.Members.size(); ++Index)
		{
			PackValue(OutBytes, InLayout.Members[Index], InValue.Elements.at(Index), Base, InDepth + 1);
		}
		return;
	}
	if (InLayout.Kind == EShaderValueKind::Array)
	{
		if (InLayout.ArrayStride == 0 || InLayout.Members.size() != 1)
		{
			throw std::invalid_argument("Invalid reflected material array layout");
		}
		for (std::uint32_t Index = 0; Index < InLayout.ArrayCount; ++Index)
		{
			PackValue(OutBytes, InLayout.Members.front(), InValue.Elements.at(Index),
			          Base + static_cast<std::uint64_t>(Index) * InLayout.ArrayStride, InDepth + 1);
		}
		return;
	}
	const bool bMatrix = InLayout.MatrixStride != 0;
	for (std::uint32_t Row = 0; Row < InLayout.Rows; ++Row)
	{
		for (std::uint32_t Column = 0; Column < InLayout.Columns; ++Column)
		{
			const std::uint64_t Offset = bMatrix ? (InLayout.bRowMajor ? Row * InLayout.MatrixStride + Column * 4ULL
			                                                           : Column * InLayout.MatrixStride + Row * 4ULL)
			                                     : (Row * InLayout.Columns + Column) * 4ULL;
			WriteWord(OutBytes, Base + Offset, InValue.Words.at(Row * InLayout.Columns + Column));
		}
	}
}
} // namespace

template<typename TValue>
std::vector<std::byte> PackConstants(const FMaterialProgramBinding& InBinding, std::span<const TValue> InValues)
{
	if (InBinding.Resource.Kind != EBindingKind::UniformBuffer || InBinding.Resource.ByteSize == 0 ||
	    InBinding.Resource.ByteSize > 65536)
	{
		throw std::invalid_argument("Cannot pack an invalid material constant buffer");
	}
	std::vector<std::byte> Result(InBinding.Resource.ByteSize);
	for (const auto& Member : InBinding.Members)
	{
		if (Member.ParameterIndex >= InValues.size())
		{
			throw std::invalid_argument("Invalid material constant parameter mapping");
		}
		if (!InValues[Member.ParameterIndex])
		{
			continue;
		}
		const FMaterialValue& Value = *InValues[Member.ParameterIndex];
		Value.Validate();
		if (GetMaterialParameterType(Member.Layout) != Value.Type)
		{
			throw std::invalid_argument("Material constant type does not match reflected layout");
		}
		PackValue(Result, Member.Layout, Value, 0);
	}
	return Result;
}

std::vector<std::byte> PackMaterialConstants(const FMaterialProgramBinding& InBinding,
                                             std::span<const std::optional<FMaterialValue>> InValues)
{
	HYP_PERF_SCOPE_C(Detail, PackMaterialConstants);
	return PackConstants(InBinding, InValues);
}

std::vector<std::byte> PackMaterialConstants(const FMaterialProgramBinding& InBinding,
                                             std::span<const std::shared_ptr<const FMaterialValue>> InValues)
{
	HYP_PERF_SCOPE_C(Detail, PackMaterialConstants);
	return PackConstants(InBinding, InValues);
}
} // namespace Hyperion
