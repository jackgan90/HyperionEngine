#include "ShaderReflection.h"
#include <Windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <bit>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <limits>
#include <stdexcept>

namespace Hyperion::ShadersPrivate
{
namespace
{
using Microsoft::WRL::ComPtr;

void Checked(HRESULT InResult, const char* InMessage)
{
	if (FAILED(InResult))
	{
		throw std::runtime_error(InMessage);
	}
}

EShaderScalar Scalar(D3D_SHADER_VARIABLE_TYPE InType)
{
	switch (InType)
	{
		case D3D_SVT_BOOL:
			return EShaderScalar::Bool;
		case D3D_SVT_INT:
			return EShaderScalar::Int;
		case D3D_SVT_UINT:
			return EShaderScalar::Uint;
		case D3D_SVT_FLOAT:
			return EShaderScalar::Float;
		default:
			return EShaderScalar::Unsupported;
	}
}

FShaderMember ReadType(ID3D12ShaderReflectionType* InType, std::string InName, bool bInElement = false,
                       std::uint32_t InDepth = 0)
{
	if (InDepth > 32)
	{
		throw std::runtime_error("Shader reflection nesting exceeds supported depth");
	}
	D3D12_SHADER_TYPE_DESC Desc{};
	Checked(InType->GetDesc(&Desc), "Read DXIL constant type");
	FShaderMember Result;
	Result.Name = std::move(InName);
	Result.Offset = bInElement ? 0 : Desc.Offset;
	if (Desc.Elements != 0 && !bInElement)
	{
		Result.Kind = EShaderValueKind::Array;
		Result.ArrayCount = Desc.Elements;
		Result.Members.push_back(ReadType(InType, "", true, InDepth + 1));
		Result.ArrayStride = (Result.Members.front().Size + 15U) & ~15U;
		const std::uint64_t Size =
		    std::uint64_t(Result.ArrayCount - 1) * Result.ArrayStride + Result.Members.front().Size;
		if (Size > 65536)
		{
			throw std::runtime_error("DXIL constant array exceeds supported range");
		}
		Result.Size = static_cast<std::uint32_t>(Size);
	}
	else if (Desc.Class == D3D_SVC_STRUCT)
	{
		Result.Kind = EShaderValueKind::Structure;
		for (UINT Index = 0; Index < Desc.Members; ++Index)
		{
			FShaderMember Member =
			    ReadType(InType->GetMemberTypeByIndex(Index), InType->GetMemberTypeName(Index), false, InDepth + 1);
			Result.Size = std::max(Result.Size, Member.Offset + Member.Size);
			Result.Members.push_back(std::move(Member));
		}
	}
	else
	{
		Result.Scalar = Scalar(Desc.Type);
		Result.Rows = Desc.Rows;
		Result.Columns = Desc.Columns;
		if (Desc.Class == D3D_SVC_MATRIX_ROWS || Desc.Class == D3D_SVC_MATRIX_COLUMNS)
		{
			Result.bRowMajor = Desc.Class == D3D_SVC_MATRIX_ROWS;
			Result.MatrixStride = 16;
			const std::uint32_t Major = Result.bRowMajor ? Desc.Rows : Desc.Columns;
			const std::uint32_t Minor = Result.bRowMajor ? Desc.Columns : Desc.Rows;
			Result.Size = (Major - 1) * Result.MatrixStride + Minor * 4;
		}
		else if (Desc.Class == D3D_SVC_SCALAR || Desc.Class == D3D_SVC_VECTOR)
		{
			Result.Size = Desc.Rows * Desc.Columns * 4;
		}
		else
		{
			Result.Scalar = EShaderScalar::Unsupported;
		}
	}
	return Result;
}

FShaderSignatureParameter ReadSignature(const D3D12_SIGNATURE_PARAMETER_DESC& InDesc)
{
	FShaderSignatureParameter Result;
	Result.Semantic = InDesc.SemanticName;
	Result.SemanticIndex = InDesc.SemanticIndex;
	Result.Location = InDesc.Register;
	Result.Components = std::popcount(static_cast<unsigned>(InDesc.Mask));
	Result.bSystemValue = InDesc.SystemValueType != D3D_NAME_UNDEFINED;
	switch (InDesc.ComponentType)
	{
		case D3D_REGISTER_COMPONENT_UINT32:
			Result.Scalar = EShaderScalar::Uint;
			break;
		case D3D_REGISTER_COMPONENT_SINT32:
			Result.Scalar = EShaderScalar::Int;
			break;
		case D3D_REGISTER_COMPONENT_FLOAT32:
			Result.Scalar = EShaderScalar::Float;
			break;
		default:
			Result.Scalar = EShaderScalar::Unsupported;
			break;
	}
	return Result;
}

FShaderBinding ReadBinding(ID3D12ShaderReflection* InReflection, const D3D12_SHADER_INPUT_BIND_DESC& InDesc,
                           EShaderStage InStage)
{
	FShaderBinding Result{};
	Result.Name = InDesc.Name;
	Result.Binding = Result.Register = InDesc.BindPoint;
	Result.Space = InDesc.Space;
	Result.Count = InDesc.BindCount;
	Result.Stage = InStage;
	switch (InDesc.Type)
	{
		case D3D_SIT_CBUFFER:
			Result.Kind = EBindingKind::UniformBuffer;
			break;
		case D3D_SIT_TEXTURE:
			Result.Kind = EBindingKind::Texture;
			break;
		case D3D_SIT_SAMPLER:
			Result.Kind = EBindingKind::Sampler;
			break;
		case D3D_SIT_STRUCTURED:
			Result.Kind = EBindingKind::StructuredBuffer;
			Result.StructureByteStride = InDesc.NumSamples;
			break;
		case D3D_SIT_BYTEADDRESS:
			Result.Kind = EBindingKind::RawBuffer;
			break;
		default:
			Result.Kind = EBindingKind::Unsupported;
			break;
	}
	if (Result.Kind == EBindingKind::Texture)
	{
		switch (InDesc.ReturnType)
		{
			case D3D_RETURN_TYPE_FLOAT:
			case D3D_RETURN_TYPE_UNORM:
			case D3D_RETURN_TYPE_SNORM:
				Result.ResourceScalar = EShaderScalar::Float;
				break;
			case D3D_RETURN_TYPE_SINT:
				Result.ResourceScalar = EShaderScalar::Int;
				break;
			case D3D_RETURN_TYPE_UINT:
				Result.ResourceScalar = EShaderScalar::Uint;
				break;
			default:
				Result.ResourceScalar = EShaderScalar::Unsupported;
				break;
		}
		Result.Dimension = InDesc.Dimension == D3D_SRV_DIMENSION_TEXTURE2D ? EShaderResourceDimension::Texture2D
		                                                                   : EShaderResourceDimension::Unsupported;
	}
	else if (Result.Kind == EBindingKind::StructuredBuffer || Result.Kind == EBindingKind::RawBuffer)
	{
		Result.Dimension = EShaderResourceDimension::Buffer;
	}
	Result.bComparison = Result.Kind == EBindingKind::Sampler && (InDesc.uFlags & D3D_SIF_COMPARISON_SAMPLER) != 0;
	if (Result.Kind == EBindingKind::UniformBuffer)
	{
		ID3D12ShaderReflectionConstantBuffer* Buffer = InReflection->GetConstantBufferByName(InDesc.Name);
		D3D12_SHADER_BUFFER_DESC Desc{};
		Checked(Buffer->GetDesc(&Desc), "Read DXIL constant buffer");
		Result.ByteSize = Desc.Size;
		for (UINT Index = 0; Index < Desc.Variables; ++Index)
		{
			ID3D12ShaderReflectionVariable* Variable = Buffer->GetVariableByIndex(Index);
			D3D12_SHADER_VARIABLE_DESC VariableDesc{};
			Checked(Variable->GetDesc(&VariableDesc), "Read DXIL constant variable");
			FShaderMember Member = ReadType(Variable->GetType(), VariableDesc.Name);
			Member.Offset = VariableDesc.StartOffset;
			Member.Size = VariableDesc.Size;
			Member.bActive = (VariableDesc.uFlags & D3D_SVF_USED) != 0;
			Result.Members.push_back(std::move(Member));
		}
	}
	return Result;
}
} // namespace

void ReflectDxil(FShaderArtifact& InArtifact, const std::string& InPayload)
{
	if (InPayload.size() > std::numeric_limits<UINT32>::max())
	{
		throw std::runtime_error("DXIL container too large");
	}
	ComPtr<IDxcUtils> Utils;
	ComPtr<IDxcBlobEncoding> Blob;
	ComPtr<IDxcContainerReflection> Container;
	ComPtr<ID3D12ShaderReflection> Reflection;
	Checked(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils)), "Create reflection utils");
	Checked(Utils->CreateBlob(InPayload.data(), static_cast<UINT32>(InPayload.size()), DXC_CP_ACP, &Blob),
	        "Create DXIL reflection blob");
	Checked(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&Container)),
	        "Create DXIL container reflection");
	Checked(Container->Load(Blob.Get()), "Load DXIL container");
	UINT32 Index{};
	Checked(Container->FindFirstPartKind(0x4c495844U, &Index), "Find DXIL reflection part");
	Checked(Container->GetPartReflection(Index, IID_PPV_ARGS(&Reflection)), "Read DXIL reflection part");
	D3D12_SHADER_DESC Desc{};
	Checked(Reflection->GetDesc(&Desc), "Read DXIL shader description");
	InArtifact.Reflection.LayoutFormat = EShaderFormat::Dxil;
	for (UINT Resource = 0; Resource < Desc.BoundResources; ++Resource)
	{
		D3D12_SHADER_INPUT_BIND_DESC Binding{};
		Checked(Reflection->GetResourceBindingDesc(Resource, &Binding), "Read DXIL resource binding");
		InArtifact.Bindings.push_back(ReadBinding(Reflection.Get(), Binding, InArtifact.Stage));
	}
	for (UINT Parameter = 0; Parameter < Desc.InputParameters; ++Parameter)
	{
		D3D12_SIGNATURE_PARAMETER_DESC Signature{};
		Checked(Reflection->GetInputParameterDesc(Parameter, &Signature), "Read DXIL input signature");
		InArtifact.Reflection.Inputs.push_back(ReadSignature(Signature));
	}
	for (UINT Parameter = 0; Parameter < Desc.OutputParameters; ++Parameter)
	{
		D3D12_SIGNATURE_PARAMETER_DESC Signature{};
		Checked(Reflection->GetOutputParameterDesc(Parameter, &Signature), "Read DXIL output signature");
		InArtifact.Reflection.Outputs.push_back(ReadSignature(Signature));
	}
}

void ValidateShaderBindings(const FShaderArtifact& InArtifact)
{
	for (const FShaderBinding& Binding : InArtifact.Bindings)
	{
		if (Binding.Space >= ShaderRegisterSpaceCount || Binding.Register >= ShaderRegistersPerKind ||
		    Binding.Count == 0 || Binding.Count > ShaderRegistersPerKind - Binding.Register)
		{
			throw std::invalid_argument("Shader binding exceeds supported register/space range: " + Binding.Name);
		}
	}
}
} // namespace Hyperion::ShadersPrivate
