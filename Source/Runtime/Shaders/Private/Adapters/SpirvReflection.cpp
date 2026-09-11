#include "ShaderReflection.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <spirv_cross.hpp>
#include <spirv_msl.hpp>
#include <stdexcept>

namespace Hyperion::ShadersPrivate
{
namespace
{
EShaderScalar Scalar(const spirv_cross::SPIRType& InType)
{
	if (InType.basetype == spirv_cross::SPIRType::Boolean)
	{
		return EShaderScalar::Bool;
	}
	if (InType.width != 32)
	{
		return EShaderScalar::Unsupported;
	}
	switch (InType.basetype)
	{
		case spirv_cross::SPIRType::Float:
			return EShaderScalar::Float;
		case spirv_cross::SPIRType::Int:
			return EShaderScalar::Int;
		case spirv_cross::SPIRType::UInt:
			return EShaderScalar::Uint;
		default:
			return EShaderScalar::Unsupported;
	}
}

const FShaderMember* FindMember(const FShaderMember* InParent, const std::string& InName)
{
	if (InParent)
	{
		for (const FShaderMember& Member : InParent->Members)
		{
			if (Member.Name == InName)
			{
				return &Member;
			}
		}
	}
	return nullptr;
}

FShaderMember ReadType(spirv_cross::Compiler& InCross, const spirv_cross::SPIRType& InType, FShaderMember InResult,
                       const FShaderMember* InLogical, std::uint32_t InDepth = 0)
{
	if (InDepth > 32)
	{
		throw std::runtime_error("SPIR-V reflection exceeds supported nesting");
	}
	if (!InType.array.empty())
	{
		if (!InType.array_size_literal.back() || InType.array.back() == 0)
		{
			throw std::runtime_error("Runtime uniform arrays are unsupported");
		}
		InResult.Kind = EShaderValueKind::Array;
		InResult.ArrayCount = InType.array.back();
		if (InResult.ArrayStride == 0)
		{
			InResult.ArrayStride = InCross.get_decoration(InType.self, spv::DecorationArrayStride);
		}
		if (InResult.ArrayStride == 0)
		{
			throw std::runtime_error("SPIR-V array stride is missing");
		}
		FShaderMember Element;
		Element.ArrayStride = InCross.get_decoration(InType.parent_type, spv::DecorationArrayStride);
		Element.MatrixStride = InResult.MatrixStride;
		Element.bRowMajor = InResult.bRowMajor;
		const FShaderMember* LogicalElement =
		    InLogical && InLogical->Kind == EShaderValueKind::Array ? &InLogical->Members.front() : InLogical;
		InResult.Members.push_back(
		    ReadType(InCross, InCross.get_type(InType.parent_type), std::move(Element), LogicalElement, InDepth + 1));
		const std::uint64_t Size = std::uint64_t(InResult.ArrayStride) * InResult.ArrayCount;
		if (Size > 65536)
		{
			throw std::runtime_error("SPIR-V uniform array exceeds supported range");
		}
		InResult.Size = static_cast<std::uint32_t>(Size);
	}
	else if (InType.basetype == spirv_cross::SPIRType::Struct)
	{
		InResult.Kind = EShaderValueKind::Structure;
		InResult.Size = static_cast<std::uint32_t>(InCross.get_declared_struct_size(InType));
		for (std::uint32_t Index = 0; Index < InType.member_types.size(); ++Index)
		{
			FShaderMember Member;
			Member.Name = InCross.get_member_name(InType.self, Index);
			Member.Offset = InCross.type_struct_member_offset(InType, Index);
			if (!InCross.get_type(InType.member_types[Index]).array.empty())
			{
				Member.ArrayStride = InCross.type_struct_member_array_stride(InType, Index);
			}
			if (InCross.has_member_decoration(InType.self, Index, spv::DecorationMatrixStride))
			{
				Member.MatrixStride = InCross.type_struct_member_matrix_stride(InType, Index);
				// DXC transposes the logical HLSL matrix when expressing it as SPIR-V.
				Member.bRowMajor = !InCross.has_member_decoration(InType.self, Index, spv::DecorationRowMajor);
			}
			const FShaderMember* Logical = FindMember(InLogical, Member.Name);
			InResult.Members.push_back(ReadType(InCross, InCross.get_type(InType.member_types[Index]),
			                                    std::move(Member), Logical, InDepth + 1));
		}
	}
	else
	{
		InResult.Scalar = Scalar(InType);
		InResult.Rows = InType.columns;
		InResult.Columns = InType.vecsize;
		if (InLogical && InLogical->Kind == EShaderValueKind::Numeric)
		{
			InResult.Rows = InLogical->Rows;
			InResult.Columns = InLogical->Columns;
			if (InLogical->Scalar == EShaderScalar::Bool && InResult.Scalar == EShaderScalar::Uint)
			{
				InResult.Scalar = EShaderScalar::Bool;
			}
		}
		if (InResult.Rows > 1)
		{
			const std::uint32_t Major = InResult.bRowMajor ? InResult.Rows : InResult.Columns;
			const std::uint32_t Minor = InResult.bRowMajor ? InResult.Columns : InResult.Rows;
			InResult.Size = (Major - 1) * InResult.MatrixStride + Minor * 4;
		}
		else
		{
			InResult.Size = InResult.Columns * 4;
		}
	}
	return InResult;
}

std::uint32_t DescriptorCount(const spirv_cross::SPIRType& InType)
{
	std::uint64_t Count = 1;
	for (std::size_t Index = 0; Index < InType.array.size(); ++Index)
	{
		if (!InType.array_size_literal[Index] || InType.array[Index] == 0)
		{
			return 0;
		}
		Count *= InType.array[Index];
		if (Count > ShaderRegistersPerKind)
		{
			throw std::runtime_error("SPIR-V descriptor array exceeds supported range");
		}
	}
	return static_cast<std::uint32_t>(Count);
}

FShaderBinding ReadBinding(spirv_cross::Compiler& InCross, const spirv_cross::Resource& InResource, EBindingKind InKind,
                           const FShaderArtifact& InLogical)
{
	FShaderBinding Result{};
	Result.Name = InResource.name;
	Result.Kind = InKind;
	Result.Binding = InCross.get_decoration(InResource.id, spv::DecorationBinding);
	Result.Space = InCross.get_decoration(InResource.id, spv::DecorationDescriptorSet);
	Result.Stage = InLogical.Stage;
	if (InKind == EBindingKind::StructuredBuffer && Result.Binding >= 3000)
	{
		InKind = EBindingKind::Unsupported;
		Result.Kind = InKind;
	}
	const std::uint32_t Shift = InKind == EBindingKind::UniformBuffer ? 0
	                            : InKind == EBindingKind::Sampler     ? 2000
	                            : InKind == EBindingKind::Unsupported ? 3000
	                                                                  : 1000;
	if (Result.Binding < Shift || Result.Binding >= Shift + ShaderRegistersPerKind)
	{
		throw std::runtime_error("SPIR-V resource register mapping collision");
	}
	Result.Register = Result.Binding - Shift;
	const spirv_cross::SPIRType& Type = InCross.get_type(InResource.type_id);
	Result.Count = DescriptorCount(Type);
	const FShaderBinding* Logical = nullptr;
	for (const FShaderBinding& Binding : InLogical.Bindings)
	{
		const bool bSameKind = Binding.Kind == InKind ||
		                       (InKind == EBindingKind::StructuredBuffer && Binding.Kind == EBindingKind::RawBuffer);
		if (Binding.Space == Result.Space && Binding.Register == Result.Register && bSameKind)
		{
			Logical = &Binding;
			Result.Name = Binding.Name;
			Result.Kind = Binding.Kind;
			Result.bComparison = Binding.bComparison;
			break;
		}
	}
	if (InKind == EBindingKind::UniformBuffer)
	{
		FShaderMember Source;
		if (Logical)
		{
			Source.Members = Logical->Members;
		}
		FShaderMember Block = ReadType(InCross, InCross.get_type(InResource.base_type_id), {}, &Source);
		Result.ByteSize = Block.Size;
		Result.Members = std::move(Block.Members);
		const auto Ranges = InCross.get_active_buffer_ranges(InResource.id);
		for (std::uint32_t Index = 0; Index < Result.Members.size(); ++Index)
		{
			Result.Members[Index].bActive = std::any_of(Ranges.begin(), Ranges.end(),
			                                            [Index](const spirv_cross::BufferRange& InRange)
			                                            {
				                                            return InRange.index == Index;
			                                            });
		}
	}
	else if (InKind == EBindingKind::Texture)
	{
		Result.ResourceScalar = Scalar(InCross.get_type(Type.image.type));
		Result.Dimension = Type.image.dim == spv::Dim2D && !Type.image.arrayed && !Type.image.ms
		                       ? EShaderResourceDimension::Texture2D
		                       : EShaderResourceDimension::Unsupported;
	}
	else if (InKind == EBindingKind::StructuredBuffer)
	{
		Result.Dimension = EShaderResourceDimension::Buffer;
		if (Result.Kind == EBindingKind::StructuredBuffer)
		{
			Result.StructureByteStride =
			    InCross.type_struct_member_array_stride(InCross.get_type(InResource.base_type_id), 0);
		}
	}
	return Result;
}

std::vector<FShaderSignatureParameter> ReadSignature(spirv_cross::Compiler& InCross,
                                                     const spirv_cross::SmallVector<spirv_cross::Resource>& InResources)
{
	std::vector<FShaderSignatureParameter> Result;
	for (const spirv_cross::Resource& Resource : InResources)
	{
		FShaderSignatureParameter Parameter;
		Parameter.Semantic = InCross.get_decoration_string(Resource.id, spv::DecorationHlslSemanticGOOGLE);
		if (Parameter.Semantic.empty())
		{
			Parameter.Semantic = Resource.name.substr(Resource.name.find_last_of('.') + 1);
		}
		const std::size_t Digit = Parameter.Semantic.find_last_not_of("0123456789");
		if (Digit != std::string::npos && Digit + 1 < Parameter.Semantic.size())
		{
			Parameter.SemanticIndex = static_cast<std::uint32_t>(std::stoul(Parameter.Semantic.substr(Digit + 1)));
			Parameter.Semantic.resize(Digit + 1);
		}
		Parameter.Location = InCross.get_decoration(Resource.id, spv::DecorationLocation);
		const spirv_cross::SPIRType& Type = InCross.get_type(Resource.type_id);
		Parameter.Scalar = Scalar(Type);
		Parameter.Components = Type.vecsize;
		Parameter.bSystemValue = Parameter.Semantic.starts_with("SV_");
		Result.push_back(std::move(Parameter));
	}
	return Result;
}
} // namespace

void ReflectSpirv(FShaderArtifact& InArtifact, std::string& InPayload, const FShaderArtifact& InLogical)
{
	if (InPayload.size() % 4 != 0)
	{
		throw std::runtime_error("Invalid SPIR-V byte count");
	}
	std::vector<std::uint32_t> Words(InPayload.size() / 4);
	std::memcpy(Words.data(), InPayload.data(), InPayload.size());
	spirv_cross::CompilerMSL Cross(std::move(Words));
	auto MslOptions = Cross.get_msl_options();
	MslOptions.set_msl_version(2, 0);
	Cross.set_msl_options(MslOptions);
	const auto Resources = Cross.get_shader_resources(Cross.get_active_interface_variables());
	std::vector<std::uint32_t> ResourceIds;
	auto Reflect = [&](const auto& InResources, EBindingKind InKind)
	{
		for (const spirv_cross::Resource& Resource : InResources)
		{
			InArtifact.Bindings.push_back(ReadBinding(Cross, Resource, InKind, InLogical));
			ResourceIds.push_back(Resource.id);
		}
	};
	Reflect(Resources.uniform_buffers, EBindingKind::UniformBuffer);
	Reflect(Resources.separate_images, EBindingKind::Texture);
	Reflect(Resources.separate_samplers, EBindingKind::Sampler);
	Reflect(Resources.storage_buffers, EBindingKind::StructuredBuffer);
	Reflect(Resources.storage_images, EBindingKind::Unsupported);
	InArtifact.Reflection.LayoutFormat = EShaderFormat::Spirv;
	InArtifact.Reflection.Inputs = ReadSignature(Cross, Resources.stage_inputs);
	InArtifact.Reflection.Outputs = ReadSignature(Cross, Resources.stage_outputs);
	for (const FShaderSignatureParameter& Parameter : InLogical.Reflection.Inputs)
	{
		if (Parameter.bSystemValue)
		{
			InArtifact.Reflection.Inputs.push_back(Parameter);
		}
	}
	for (const FShaderSignatureParameter& Parameter : InLogical.Reflection.Outputs)
	{
		if (Parameter.bSystemValue &&
		    !std::any_of(InArtifact.Reflection.Outputs.begin(), InArtifact.Reflection.Outputs.end(),
		                 [&Parameter](const FShaderSignatureParameter& InOther)
		                 {
			                 return std::equal(InOther.Semantic.begin(), InOther.Semantic.end(),
			                                   Parameter.Semantic.begin(), Parameter.Semantic.end(),
			                                   [](unsigned char InLeft, unsigned char InRight)
			                                   {
				                                   return std::toupper(InLeft) == std::toupper(InRight);
			                                   }) &&
			                        InOther.SemanticIndex == Parameter.SemanticIndex;
		                 }))
		{
			InArtifact.Reflection.Outputs.push_back(Parameter);
		}
	}
	if (InArtifact.Format == EShaderFormat::Msl)
	{
		InPayload = Cross.compile();
		for (std::size_t Index = 0; Index < ResourceIds.size(); ++Index)
		{
			InArtifact.Bindings[Index].MslBinding = Cross.get_automatic_msl_resource_binding(ResourceIds[Index]);
		}
	}
}
} // namespace Hyperion::ShadersPrivate
