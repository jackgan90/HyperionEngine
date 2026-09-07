#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <functional>

using namespace Hyperion;

namespace
{
const FShaderBinding& FindBinding(const FShaderArtifact& InArtifact, const std::string& InName)
{
	const auto Found = std::find_if(InArtifact.Bindings.begin(), InArtifact.Bindings.end(),
	                                [&InName](const FShaderBinding& InBinding)
	                                {
		                                return InBinding.Name == InName;
	                                });
	HYP_CHECK(Found != InArtifact.Bindings.end());
	return *Found;
}

const FShaderMember& FindMember(const std::vector<FShaderMember>& InMembers, const std::string& InName)
{
	const auto Found = std::find_if(InMembers.begin(), InMembers.end(),
	                                [&InName](const FShaderMember& InMember)
	                                {
		                                return InMember.Name == InName;
	                                });
	HYP_CHECK(Found != InMembers.end());
	return *Found;
}

void ExpectError(const std::function<void()>& InAction, std::string_view InText)
{
	try
	{
		InAction();
	}
	catch (const std::exception& Error)
	{
		HYP_CHECK(std::string_view(Error.what()).find(InText) != std::string_view::npos);
		return;
	}
	throw std::runtime_error("Expected shader reflection error");
}

void CheckMembers(const FShaderArtifact& InArtifact)
{
	const FShaderBinding& Block = FindBinding(InArtifact, "Parameters");
	HYP_CHECK(Block.Register == 2 && Block.Space == 1 && Block.ByteSize >= 224);
	const FShaderMember& Nested = FindMember(Block.Members, "Nested");
	HYP_CHECK(Nested.Kind == EShaderValueKind::Structure);
	HYP_CHECK(FindMember(Nested.Members, "Enabled").Scalar == EShaderScalar::Bool);
	HYP_CHECK(FindMember(Nested.Members, "Pair").Scalar == EShaderScalar::Int);
	const FShaderMember& Array = FindMember(Block.Members, "Values");
	HYP_CHECK(Array.Kind == EShaderValueKind::Array && Array.ArrayCount == 3 && Array.ArrayStride == 16);
	const FShaderMember& RowMatrix = FindMember(Block.Members, "RowMatrix");
	const FShaderMember& ColumnMatrix = FindMember(Block.Members, "ColumnMatrix");
	HYP_CHECK(RowMatrix.Rows == 2 && RowMatrix.Columns == 3 && RowMatrix.bRowMajor && RowMatrix.MatrixStride == 16);
	HYP_CHECK(ColumnMatrix.Rows == 3 && ColumnMatrix.Columns == 2 && !ColumnMatrix.bRowMajor &&
	          ColumnMatrix.MatrixStride == 16);
	const FShaderMember& Matrices = FindMember(Block.Members, "Matrices");
	HYP_CHECK(Matrices.ArrayCount == 2 && Matrices.ArrayStride == 32 && Matrices.Members.front().bRowMajor);
	const FShaderBinding& Textures = FindBinding(InArtifact, "Maps");
	HYP_CHECK(Textures.Count == 8 && Textures.Register == 3 && Textures.Space == 2);
	HYP_CHECK(FindBinding(InArtifact, "Data").Kind == EBindingKind::StructuredBuffer);
	HYP_CHECK(FindBinding(InArtifact, "Data").StructureByteStride == 16);
	HYP_CHECK(FindBinding(InArtifact, "Raw").Kind == EBindingKind::RawBuffer);
	if (InArtifact.Format == EShaderFormat::Msl)
	{
		HYP_CHECK(Textures.MslBinding != 0xffffffffU && InArtifact.Reflection.LayoutFormat == EShaderFormat::Spirv);
	}
}

void CheckTextureComponents(const std::filesystem::path& InRoot)
{
	std::ofstream(InRoot / "TextureComponents.hlsl") << R"(
Texture2D<COMPONENT_TYPE> Image : register(t0);
float4 PSMain() : SV_Target0 { return float4(Image.Load(int3(0,0,0))); }
)";
	FShaderCompiler Compiler(InRoot, "material-shader-test/components");
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		for (const auto& [Type, Scalar] :
		     std::array{std::pair{"float4", EShaderScalar::Float}, std::pair{"int4", EShaderScalar::Int},
		                std::pair{"uint4", EShaderScalar::Uint}})
		{
			const FShaderCompileOptions Options{{{"COMPONENT_TYPE", Type}}};
			const auto First =
			    Compiler.Compile("TextureComponents.hlsl", "PSMain", EShaderStage::Pixel, Format, Options);
			HYP_CHECK(FindBinding(First, "Image").ResourceScalar == Scalar);
			const auto Hot = Compiler.Compile("TextureComponents.hlsl", "PSMain", EShaderStage::Pixel, Format, Options);
			HYP_CHECK(Hot.bCacheHit && Hot.Bindings == First.Bindings && Hot.Reflection == First.Reflection);
		}
	}
}

void CheckNestedArrayTypes(const std::filesystem::path& InRoot)
{
	std::ofstream(InRoot / "ArrayTypes.hlsl") << R"(
cbuffer Arrays : register(b0) { bool Flags[2][3]; float Grid[2][3]; uint Index; };
float4 PSMain() : SV_Target0 { return Grid[Index % 2][Index % 3] + (Flags[Index % 2][Index % 3] ? 1 : 0); }
)";
	FShaderCompiler Compiler(InRoot, "material-shader-test/arrays");
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const auto First = Compiler.Compile("ArrayTypes.hlsl", "PSMain", EShaderStage::Pixel, Format);
		const auto& Block = FindBinding(First, "Arrays");
		const auto& Flags = FindMember(Block.Members, "Flags");
		const auto& Grid = FindMember(Block.Members, "Grid");
		if (Format == EShaderFormat::Dxil)
		{
			HYP_CHECK(Flags.ArrayCount == 6 && Flags.Members.front().Scalar == EShaderScalar::Bool);
			HYP_CHECK(Grid.ArrayCount == 6 && Grid.Members.front().Scalar == EShaderScalar::Float);
		}
		else
		{
			HYP_CHECK(Flags.ArrayCount == 2 && Flags.Members.front().ArrayCount == 3);
			HYP_CHECK(Flags.Members.front().Members.front().Scalar == EShaderScalar::Bool);
			HYP_CHECK(Grid.Members.front().Members.front().Scalar == EShaderScalar::Float);
		}
		const auto Hot = Compiler.Compile("ArrayTypes.hlsl", "PSMain", EShaderStage::Pixel, Format);
		HYP_CHECK(Hot.bCacheHit && Hot.Bindings == First.Bindings && Hot.Reflection == First.Reflection);
	}
}
} // namespace

void CheckMaterialShaderReflection(const std::filesystem::path& InRoot)
{
	CheckTextureComponents(InRoot);
	CheckNestedArrayTypes(InRoot);
	{
		std::ofstream Shader(InRoot / "MaterialReflection.hlsl");
		Shader << R"(
struct FInner { float3 Direction; bool Enabled; int2 Pair; };
cbuffer Parameters : register(b2, space1)
{
    float Strength;
    FInner Nested;
    float Values[3];
    row_major float2x3 RowMatrix;
    column_major float3x2 ColumnMatrix;
    row_major float2x3 Matrices[2];
    uint Index;
    float Unused;
};
Texture2D Maps[8] : register(t3, space2);
SamplerState SurfaceSampler : register(s3, space2);
StructuredBuffer<float4> Data : register(t16, space1);
ByteAddressBuffer Raw : register(t2, space2);
float4 PSMain(float4 InPosition : SV_Position) : SV_Target0
{
    float Value = Strength + dot(Nested.Direction, float3(1,2,3)) + (Nested.Enabled ? 7 : 9)
        + Nested.Pair.x + Nested.Pair.y + Values[Index % 3] + RowMatrix[Index % 2][Index % 3]
        + ColumnMatrix[Index % 3][Index % 2] + Matrices[Index % 2][Index % 2][Index % 3];
    return Maps[Index % 8].SampleLevel(SurfaceSampler, InPosition.xy * .01, 0)
        + Data[Index] + float4(Raw.Load(Index * 4), Value * FACTOR, 0, 1);
}
)";
	}
	const auto RunIdentity = std::chrono::steady_clock::now().time_since_epoch().count();
	FShaderCompiler Compiler(InRoot, std::filesystem::path("material-shader-test/cache") / std::to_string(RunIdentity));
	FShaderCompileOptions Options{{{"FACTOR", "1"}}};
	for (EShaderFormat Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const FShaderArtifact First =
		    Compiler.Compile("MaterialReflection.hlsl", "PSMain", EShaderStage::Pixel, Format, Options);
		HYP_CHECK(!First.bCacheHit);
		CheckMembers(First);
		const FShaderArtifact Hot =
		    Compiler.Compile("MaterialReflection.hlsl", "PSMain", EShaderStage::Pixel, Format, Options);
		HYP_CHECK(Hot.bCacheHit && Hot.Bytes == First.Bytes && Hot.Bindings == First.Bindings &&
		          Hot.Reflection == First.Reflection);
		FShaderCompileOptions Changed = Options;
		Changed.Defines.front().Value = "2";
		const FShaderArtifact Different =
		    Compiler.Compile("MaterialReflection.hlsl", "PSMain", EShaderStage::Pixel, Format, Changed);
		HYP_CHECK(Different.CacheKey != Hot.CacheKey && Different.Bytes != Hot.Bytes);
	}
	Options.Defines.push_back(Options.Defines.front());
	ExpectError(
	    [&]
	    {
		    Compiler.Compile("MaterialReflection.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, Options);
	    },
	    "duplicate");
	{
		std::ofstream Shader(InRoot / "InvalidSpace.hlsl");
		Shader
		    << "cbuffer Bad : register(b0, space4) { float4 Color; }; float4 PSMain() : SV_Target0 { return Color; }";
	}
	ExpectError(
	    [&]
	    {
		    Compiler.Compile("InvalidSpace.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Spirv);
	    },
	    "register/space");
}
