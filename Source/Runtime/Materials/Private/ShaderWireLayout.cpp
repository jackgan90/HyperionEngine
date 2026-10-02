#include "Hyperion/Materials/ShaderWireLayout.h"
#include <stdexcept>

namespace Hyperion::ShaderWirePrivate
{
namespace
{
[[noreturn]] void Reject(const FEngineMaterialResource& InContract, std::string_view InReason)
{
	throw std::invalid_argument("Shader wire layout mismatch: " + std::string(InContract.Semantic.GetName()) + "; " +
	                            std::string(InReason));
}

void ValidateStorage(const FEngineMaterialResource& InContract, std::size_t InRecordSize, bool bInDirectUpload)
{
	if (!bInDirectUpload)
	{
		Reject(InContract, "C++ record must be standard-layout and trivially-copyable");
	}
	if (InContract.Kind != EMaterialValueKind::ReadBuffer || InContract.StructureByteStride == 0 ||
	    InContract.ElementLayout.Size != InContract.StructureByteStride)
	{
		Reject(InContract, "expected a complete structured-buffer contract");
	}
	if (InRecordSize != InContract.StructureByteStride)
	{
		Reject(InContract, "C++ extent differs from the declared stride");
	}
}

void ValidateMember(const FEngineMaterialResource& InContract, const FShaderWireMember& InActual,
                    const FStandardMaterialBlockMember& InExpected)
{
	const auto& Type = InActual.Type;
	if (!Type.bSupported)
	{
		Reject(InContract, "unsupported C++ physical member: " + std::string(InActual.Name));
	}
	if ((InExpected.Scalar != EMaterialScalar::Float && InExpected.Scalar != EMaterialScalar::Uint) ||
	    InExpected.Rows != 1 || InExpected.Columns < 1 || InExpected.Columns > 4)
	{
		Reject(InContract, "unsupported declared physical member: " + InExpected.Name);
	}
	if (InActual.Name != InExpected.Name || InActual.Offset != InExpected.Offset || Type.Scalar != InExpected.Scalar ||
	    Type.Columns != InExpected.Columns || Type.Rows != InExpected.Rows || Type.Size != InExpected.Columns * 4 ||
	    InActual.Offset > InContract.StructureByteStride ||
	    Type.Size > InContract.StructureByteStride - InActual.Offset)
	{
		Reject(InContract, "member name/type/offset/extent differs: " + std::string(InActual.Name));
	}
}

bool IsUintWord(const FShaderWireMember& InMember, std::size_t InOffset)
{
	return !InMember.Name.empty() && InMember.Offset == InOffset && InMember.Type.bSupported &&
	       InMember.Type.Scalar == EMaterialScalar::Uint && InMember.Type.Columns == 1 && InMember.Type.Rows == 1 &&
	       InMember.Type.Size == 4;
}
} // namespace

std::uint32_t ValidateRecord(const FEngineMaterialResource& InContract, std::size_t InRecordSize, bool bInDirectUpload,
                             std::span<const FShaderWireMember> InMembers)
{
	ValidateStorage(InContract, InRecordSize, bInDirectUpload);
	if (InMembers.empty() || InMembers.size() != InContract.ElementLayout.Members.size())
	{
		Reject(InContract, "member count differs");
	}
	for (std::size_t Index = 0; Index < InMembers.size(); ++Index)
	{
		ValidateMember(InContract, InMembers[Index], InContract.ElementLayout.Members[Index]);
	}
	return InContract.StructureByteStride;
}

std::uint32_t ValidateUintPair(const FEngineMaterialResource& InContract, std::size_t InRecordSize,
                               bool bInDirectUpload, const FShaderWireMember& InFirst,
                               const FShaderWireMember& InSecond)
{
	ValidateStorage(InContract, InRecordSize, bInDirectUpload);
	if (InRecordSize != 8 || !IsUintWord(InFirst, 0) || !IsUintWord(InSecond, 4) || InFirst.Name == InSecond.Name)
	{
		Reject(InContract, "uint2 adapter requires two distinct contiguous uint32 fields");
	}
	const std::array<FShaderWireMember, 1> Member{{{"", 0, {EMaterialScalar::Uint, 2, 1, 8, true}}}};
	return ValidateRecord(InContract, InRecordSize, bInDirectUpload, Member);
}

std::uint32_t ValidateScalar(const FEngineMaterialResource& InContract, std::size_t InRecordSize, bool bInDirectUpload,
                             const FShaderWireType& InType)
{
	ValidateStorage(InContract, InRecordSize, bInDirectUpload);
	if (!InType.bSupported || InType.Columns != 1 || InType.Rows != 1 || InType.Size != 4)
	{
		Reject(InContract, "scalar adapter requires supported four-byte scalar storage");
	}
	const std::array<FShaderWireMember, 1> Member{{{"", 0, InType}}};
	return ValidateRecord(InContract, InRecordSize, bInDirectUpload, Member);
}
} // namespace Hyperion::ShaderWirePrivate
