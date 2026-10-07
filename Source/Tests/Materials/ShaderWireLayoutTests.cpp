#include "Hyperion/Materials/ShaderParameterLayout.h"
#include "Hyperion/Materials/ShaderWireLayout.h"
#include "Hyperion/Renderer/ClusteredLights.h"
#include "Support/TestSupport.h"
#include <array>
#include <cstddef>
#include <type_traits>

namespace
{
using namespace Hyperion;

struct FReorderedClusterLight
{
	FVec4 RadianceType;
	FVec4 PositionRange;
	FVec4 DirectionInner;
	FVec4 Outer;
};

struct FExtendedClusterLight
{
	FVec4 PositionRange;
	FVec4 RadianceType;
	FVec4 DirectionInner;
	FVec4 Outer;
	std::uint32_t Tail{};
};

struct FShortClusterMember
{
	float PositionRange{};
	FVec4 RadianceType;
	FVec4 DirectionInner;
	FVec4 Outer;
	std::array<std::uint32_t, 3> Tail{};
};

struct FDirectionalRecord
{
	FVec4 Direction;
	FVec4 Radiance;
};

struct FReorderedHeader
{
	std::uint32_t Count{};
	std::uint32_t Offset{};
};

struct FPaddedHeader
{
	std::uint32_t Offset{};
	std::uint32_t Reserved{};
	std::uint32_t Count{};
};

struct FFloatHeader
{
	float Offset{};
	std::uint32_t Count{};
};

struct FNontrivialClusterLight
{
	FVec4 PositionRange;
	FVec4 RadianceType;
	FVec4 DirectionInner;
	FVec4 Outer;

	~FNontrivialClusterLight()
	{
	}
};

struct FClusterBase
{
	FVec4 PositionRange;
};

struct FNonstandardClusterLight : FClusterBase
{
	FVec4 RadianceType;
	FVec4 DirectionInner;
	FVec4 Outer;
};

struct alignas(4) FBooleanStorage
{
	bool bValue{};
	std::array<std::byte, 3> Padding{};
};

struct FMatrixStorage
{
	FMat4 Value;
};

struct FCArrayStorage
{
	float Value[4]{};
};

struct FArrayStorage
{
	std::array<float, 4> Value{};
};

struct FUnknownVector
{
	float X{};
	float Y{};
	float Z{};
	float W{};
};

struct FUnknownStorage
{
	FUnknownVector Value;
};

static_assert(sizeof(FReorderedClusterLight) == 64 && sizeof(FShortClusterMember) == 64);
static_assert(sizeof(FReorderedHeader) == 8 && sizeof(FFloatHeader) == 8);
static_assert(sizeof(FBooleanStorage) == 4 && sizeof(FMatrixStorage) == 64);
static_assert(sizeof(FCArrayStorage) == 16 && sizeof(FArrayStorage) == 16 && sizeof(FUnknownStorage) == 16);
static_assert(std::is_standard_layout_v<FNontrivialClusterLight> &&
              !std::is_trivially_copyable_v<FNontrivialClusterLight>);
static_assert(!std::is_standard_layout_v<FNonstandardClusterLight> &&
              std::is_trivially_copyable_v<FNonstandardClusterLight>);
static_assert(!std::is_convertible_v<TShaderWireMember<FClusterLightData>, TShaderWireMember<FReorderedClusterLight>>);

template<typename T> void Reject(T InOperation, std::string_view InReason)
{
	try
	{
		InOperation();
	}
	catch (const std::invalid_argument& Error)
	{
		HYP_CHECK(std::string_view(Error.what()).find(InReason) != std::string_view::npos);
		return;
	}
	throw std::runtime_error("Expected shader wire rejection: " + std::string(InReason));
}

template<typename T> auto ClusterMembers()
{
	return std::array{HYP_SHADER_WIRE_MEMBER(T, PositionRange), HYP_SHADER_WIRE_MEMBER(T, RadianceType),
	                  HYP_SHADER_WIRE_MEMBER(T, DirectionInner), HYP_SHADER_WIRE_MEMBER(T, Outer)};
}

FEngineMaterialResource SingleFieldContract(std::string InName, EMaterialScalar InScalar, std::uint32_t InColumns,
                                            std::uint32_t InExtent)
{
	FShaderParameterLayoutBuilder Builder("Wire rejection fixture", {}, EShaderPackingProfile::Structured);
	Builder.AddField(std::move(InName), FMaterialParameterType::Numeric(InScalar, InColumns));
	Builder.SetMinimumSize(InExtent);
	FEngineMaterialResource Result;
	Result.Kind = EMaterialValueKind::ReadBuffer;
	Result.ElementLayout = Builder.Build();
	Result.StructureByteStride = Result.ElementLayout.Size;
	return Result;
}

void CheckLightingLayouts()
{
	const auto Cluster = GetEngineMaterialResource("ClusterLights");
	const auto Header = GetEngineMaterialResource("ClusterHeaders");
	const auto Index = GetEngineMaterialResource("ClusterIndices");
	const auto Directional = GetEngineMaterialResource("HyperionDirectionalLightsV1");
	HYP_CHECK(ValidateShaderWireRecord(Cluster, ClusterMembers<FClusterLightData>()) == 64);
	HYP_CHECK(ValidateShaderWireUintPair(Header, HYP_SHADER_WIRE_MEMBER(FClusterHeader, Offset),
	                                     HYP_SHADER_WIRE_MEMBER(FClusterHeader, Count)) == 8);
	HYP_CHECK(ValidateShaderWireScalar<std::uint32_t>(Index) == 4);
	const std::array Members{HYP_SHADER_WIRE_MEMBER(FDirectionalRecord, Direction),
	                         HYP_SHADER_WIRE_MEMBER(FDirectionalRecord, Radiance)};
	HYP_CHECK(ValidateShaderWireRecord(Directional, Members) == 32);
	HYP_CHECK(GetShaderWireType<FVec4>().bSupported && GetShaderWireType<FVec4>().Size == 16);
	const auto Float = SingleFieldContract("", EMaterialScalar::Float, 1, 4);
	HYP_CHECK(ValidateShaderWireScalar<float>(Float) == 4);
	Reject(
	    [&]
	    {
		    ValidateShaderWireScalar<float>(Index);
	    },
	    "member name/type/offset/extent differs");
}

void CheckRecordRejection()
{
	const auto Cluster = GetEngineMaterialResource("ClusterLights");
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Cluster, ClusterMembers<FReorderedClusterLight>());
	    },
	    "member name/type/offset/extent differs");
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Cluster, ClusterMembers<FShortClusterMember>());
	    },
	    "member name/type/offset/extent differs");
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Cluster, ClusterMembers<FExtendedClusterLight>());
	    },
	    "declared stride");
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Cluster, ClusterMembers<FNontrivialClusterLight>());
	    },
	    "standard-layout and trivially-copyable");
	// Avoid offsetof on a non-standard-layout type; record eligibility must fail before member comparison.
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Cluster, std::array<TShaderWireMember<FNonstandardClusterLight>, 0>{});
	    },
	    "standard-layout and trivially-copyable");
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Cluster, std::array<TShaderWireMember<FClusterLightData>, 0>{});
	    },
	    "member count differs");
	auto BadContract = Cluster;
	BadContract.ElementLayout.Size = 0;
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(BadContract, ClusterMembers<FClusterLightData>());
	    },
	    "complete structured-buffer contract");
}

void CheckHeaderRejection()
{
	const auto Header = GetEngineMaterialResource("ClusterHeaders");
	Reject(
	    [&]
	    {
		    ValidateShaderWireUintPair(Header, HYP_SHADER_WIRE_MEMBER(FReorderedHeader, Offset),
		                               HYP_SHADER_WIRE_MEMBER(FReorderedHeader, Count));
	    },
	    "two distinct contiguous uint32 fields");
	Reject(
	    [&]
	    {
		    ValidateShaderWireUintPair(Header, HYP_SHADER_WIRE_MEMBER(FFloatHeader, Offset),
		                               HYP_SHADER_WIRE_MEMBER(FFloatHeader, Count));
	    },
	    "two distinct contiguous uint32 fields");
	Reject(
	    [&]
	    {
		    ValidateShaderWireUintPair(Header, HYP_SHADER_WIRE_MEMBER(FPaddedHeader, Offset),
		                               HYP_SHADER_WIRE_MEMBER(FPaddedHeader, Count));
	    },
	    "declared stride");
	Reject(
	    [&]
	    {
		    ValidateShaderWireUintPair(Header, HYP_SHADER_WIRE_MEMBER(FClusterHeader, Offset),
		                               HYP_SHADER_WIRE_MEMBER(FClusterHeader, Offset));
	    },
	    "two distinct contiguous uint32 fields");
	const auto NamedPair = SingleFieldContract("Named", EMaterialScalar::Uint, 2, 8);
	Reject(
	    [&]
	    {
		    ValidateShaderWireUintPair(NamedPair, HYP_SHADER_WIRE_MEMBER(FClusterHeader, Offset),
		                               HYP_SHADER_WIRE_MEMBER(FClusterHeader, Count));
	    },
	    "member name/type/offset/extent differs");
	Reject(
	    [&]
	    {
		    ValidateShaderWireScalar<std::array<std::uint32_t, 2>>(Header);
	    },
	    "supported four-byte scalar storage");
}

void CheckUnsupportedStorage()
{
	const auto Boolean = SingleFieldContract("bValue", EMaterialScalar::Bool, 1, 4);
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Boolean, std::array{HYP_SHADER_WIRE_MEMBER(FBooleanStorage, bValue)});
	    },
	    "unsupported C++ physical member");
	const auto MatrixExtent = SingleFieldContract("Value", EMaterialScalar::Float, 4, 64);
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(MatrixExtent, std::array{HYP_SHADER_WIRE_MEMBER(FMatrixStorage, Value)});
	    },
	    "unsupported C++ physical member");
	const auto Vector = SingleFieldContract("Value", EMaterialScalar::Float, 4, 16);
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Vector, std::array{HYP_SHADER_WIRE_MEMBER(FCArrayStorage, Value)});
	    },
	    "unsupported C++ physical member");
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Vector, std::array{HYP_SHADER_WIRE_MEMBER(FArrayStorage, Value)});
	    },
	    "unsupported C++ physical member");
	Reject(
	    [&]
	    {
		    ValidateShaderWireRecord(Vector, std::array{HYP_SHADER_WIRE_MEMBER(FUnknownStorage, Value)});
	    },
	    "unsupported C++ physical member");
	Reject(
	    [&]
	    {
		    ValidateShaderWireScalar<FVec4>(Vector);
	    },
	    "supported four-byte scalar storage");
}
} // namespace

void RunShaderWireLayoutTests()
{
	CheckLightingLayouts();
	CheckRecordRejection();
	CheckHeaderRejection();
	CheckUnsupportedStorage();
}
