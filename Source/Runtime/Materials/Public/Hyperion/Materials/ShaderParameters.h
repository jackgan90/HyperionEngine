#pragma once
#include "Hyperion/Materials/Material.h"
#include "Hyperion/Materials/MaterialBlocks.h"
#include "Hyperion/Materials/ShaderParameterLayout.h"
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace Hyperion
{
std::string GenerateEngineShaderDeclarations();
std::string GenerateShaderDeclarations(const FShaderParameterContractSet& InContracts);
std::vector<std::pair<std::string, std::string>> GenerateShaderIncludes(
    std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts = {});

template<EMaterialScalar Scalar, unsigned Columns, unsigned Rows> struct TShaderValueType;

template<> struct TShaderValueType<EMaterialScalar::Float, 1, 1>
{
	using FType = float;
};

inline FMaterialValue MakeShaderValue(float InValue)
{
	return FMaterialValue::Float(InValue);
}

template<> struct TShaderValueType<EMaterialScalar::Uint, 1, 1>
{
	using FType = std::uint32_t;
};

inline FMaterialValue MakeShaderValue(std::uint32_t InValue)
{
	return FMaterialValue::Uint(InValue);
}

template<> struct TShaderValueType<EMaterialScalar::Int, 1, 1>
{
	using FType = std::int32_t;
};

inline FMaterialValue MakeShaderValue(std::int32_t InValue)
{
	return FMaterialValue::Int(InValue);
}

template<> struct TShaderValueType<EMaterialScalar::Bool, 1, 1>
{
	using FType = bool;
};

inline FMaterialValue MakeShaderValue(bool bInValue)
{
	return FMaterialValue::Bool(bInValue);
}

template<> struct TShaderValueType<EMaterialScalar::Float, 2, 1>
{
	using FType = FVec2;
};

inline FMaterialValue MakeShaderValue(FVec2 InValue)
{
	return FMaterialValue::Float(InValue);
}

template<> struct TShaderValueType<EMaterialScalar::Float, 3, 1>
{
	using FType = FVec3;
};

inline FMaterialValue MakeShaderValue(FVec3 InValue)
{
	return FMaterialValue::Float(InValue);
}

template<> struct TShaderValueType<EMaterialScalar::Float, 4, 1>
{
	using FType = FVec4;
};

inline FMaterialValue MakeShaderValue(FVec4 InValue)
{
	return FMaterialValue::Float(InValue);
}

template<> struct TShaderValueType<EMaterialScalar::Float, 4, 4>
{
	using FType = FMat4;
};

inline FMaterialValue MakeShaderValue(const FMat4& InValue)
{
	return FMaterialValue::Matrix(InValue);
}

template<typename T> FMaterialParameterValues MakeShaderParameters(const T& InParameters)
{
	FMaterialParameterValues Values;
	AppendShaderParameters(Values, InParameters);
	return Values;
}
} // namespace Hyperion

namespace Hyperion
{
template<EMaterialScalar Scalar, unsigned Columns, unsigned Rows>
struct TShaderFieldType : TShaderValueType<Scalar, Columns, Rows>
{
	static FMaterialParameterType GetType()
	{
		return FMaterialParameterType::Numeric(Scalar, Columns, Rows);
	}
};

template<> struct TShaderValueType<EMaterialScalar::Uint, 2, 1>
{
	using FType = std::array<std::uint32_t, 2>;
};

inline FMaterialValue MakeShaderValue(const std::array<std::uint32_t, 2>& InValue)
{
	FMaterialValue Result;
	Result.Type = FMaterialParameterType::Numeric(EMaterialScalar::Uint, 2);
	Result.Words.assign(InValue.begin(), InValue.end());
	return Result;
}

using FShaderFloat = TShaderFieldType<EMaterialScalar::Float, 1, 1>;
using FShaderFloat2 = TShaderFieldType<EMaterialScalar::Float, 2, 1>;
using FShaderFloat3 = TShaderFieldType<EMaterialScalar::Float, 3, 1>;
using FShaderFloat4 = TShaderFieldType<EMaterialScalar::Float, 4, 1>;
using FShaderFloat4x4 = TShaderFieldType<EMaterialScalar::Float, 4, 4>;
using FShaderUint = TShaderFieldType<EMaterialScalar::Uint, 1, 1>;
using FShaderUint2 = TShaderFieldType<EMaterialScalar::Uint, 2, 1>;
using FShaderInt = TShaderFieldType<EMaterialScalar::Int, 1, 1>;
using FShaderBool = TShaderFieldType<EMaterialScalar::Bool, 1, 1>;
} // namespace Hyperion

#define HYP_SHADER_TYPE_float FShaderFloat
#define HYP_SHADER_TYPE_float2 FShaderFloat2
#define HYP_SHADER_TYPE_float3 FShaderFloat3
#define HYP_SHADER_TYPE_float4 FShaderFloat4
#define HYP_SHADER_TYPE_float4x4 FShaderFloat4x4
#define HYP_SHADER_TYPE_uint FShaderUint
#define HYP_SHADER_TYPE_uint2 FShaderUint2
#define HYP_SHADER_TYPE_int FShaderInt
#define HYP_SHADER_TYPE_bool FShaderBool

#define HYP_SHADER_DOMAIN Engine
#define HYP_SHADER_COMMON
#define HYP_SHADER_DECLARATIONS "Hyperion/Materials/EngineSemantics.inl"
#include "Hyperion/Materials/ShaderParameterDeclarations.inl"
#undef HYP_SHADER_DECLARATIONS
#undef HYP_SHADER_COMMON
#undef HYP_SHADER_DOMAIN

namespace Hyperion
{
inline std::string_view GetEngineUniformName(EEngineUniform InUniform)
{
	return GetShaderUniformName(InUniform);
}
} // namespace Hyperion
