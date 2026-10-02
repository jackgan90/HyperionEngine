#include "Hyperion/Materials/Lighting/ClusterParameters.h"
#include "Hyperion/Materials/Lighting/SceneLightingParameters.h"
#include "Hyperion/Renderer/MaterialBlocks.h"
#include "Support/RendererShaderSupport.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <array>

namespace
{
using namespace Hyperion;

void CheckVectorRecord(const FShaderBinding& InBinding, std::span<const std::string_view> InNames,
                       std::uint32_t InStride)
{
	HYP_CHECK(InBinding.Kind == EBindingKind::StructuredBuffer && InBinding.StructureByteStride == InStride);
	HYP_CHECK(InBinding.Members.size() == 1 && InBinding.Members.front().Kind == EShaderValueKind::Structure);
	const auto& Members = InBinding.Members.front().Members;
	HYP_CHECK(Members.size() == InNames.size());
	for (std::size_t Index = 0; Index < InNames.size(); ++Index)
	{
		const auto& Member = Members[Index];
		HYP_CHECK(Member.Name == InNames[Index] && Member.Offset == Index * 16 && Member.Size == 16);
		HYP_CHECK(Member.Kind == EShaderValueKind::Numeric && Member.Scalar == EShaderScalar::Float &&
		          Member.Rows == 1 && Member.Columns == 4 && Member.ArrayCount == 0 && Member.ArrayStride == 0 &&
		          Member.MatrixStride == 0);
	}
}

void CheckAnonymousUint(const FShaderBinding& InBinding, std::uint32_t InColumns, std::uint32_t InStride)
{
	HYP_CHECK(InBinding.Kind == EBindingKind::StructuredBuffer && InBinding.StructureByteStride == InStride);
	HYP_CHECK(InBinding.Members.size() == 1);
	const auto& Member = InBinding.Members.front();
	HYP_CHECK(Member.Kind == EShaderValueKind::Numeric && Member.Scalar == EShaderScalar::Uint && Member.Offset == 0 &&
	          Member.Size == InStride && Member.Rows == 1 && Member.Columns == InColumns && Member.ArrayCount == 0 &&
	          Member.ArrayStride == 0 && Member.MatrixStride == 0);
}

const char* TargetName(EShaderFormat InFormat)
{
	switch (InFormat)
	{
		case EShaderFormat::Dxil:
			return "DXIL";
		case EShaderFormat::Spirv:
			return "SPIR-V";
		case EShaderFormat::Msl:
			return "MSL";
	}
	return "Unknown";
}

void CheckLightingBindings(const FShaderArtifact& InArtifact, std::string_view InSource, std::uint32_t InExpectedMask)
{
	std::uint32_t Mask = 0;
	for (const auto& Binding : InArtifact.Bindings)
	{
		if (Binding.Name == "ClusterLights")
		{
			const std::array<std::string_view, 4> Names{"PositionRange", "RadianceType", "DirectionInner", "Outer"};
			CheckVectorRecord(Binding, Names, 64);
			HYP_CHECK(ValidateEngineMaterialResource(Binding).Semantic == EClusterSemantic::ClusterLights);
			Mask |= 1;
		}
		else if (Binding.Name == "ClusterHeaders")
		{
			CheckAnonymousUint(Binding, 2, 8);
			HYP_CHECK(ValidateEngineMaterialResource(Binding).Semantic == EClusterSemantic::ClusterHeaders);
			Mask |= 2;
		}
		else if (Binding.Name == "ClusterIndices")
		{
			CheckAnonymousUint(Binding, 1, 4);
			HYP_CHECK(ValidateEngineMaterialResource(Binding).Semantic == EClusterSemantic::ClusterIndices);
			Mask |= 4;
		}
		else if (Binding.Name == "HyperionDirectionalLightsV1")
		{
			const std::array<std::string_view, 2> Names{"Direction", "Radiance"};
			CheckVectorRecord(Binding, Names, 32);
			HYP_CHECK(ValidateEngineMaterialResource(Binding).Semantic == ESceneLightingSemantic::DirectionalLights);
			Mask |= 8;
		}
	}
	if (Mask != InExpectedMask)
	{
		throw std::runtime_error("Lighting wire resource mask mismatch; source='" + std::string(InSource) +
		                         "'; target=" + TargetName(InArtifact.Format) + "; actual=" + std::to_string(Mask) +
		                         "; expected=" + std::to_string(InExpectedMask));
	}
}

void CheckVolumeContract(const FShaderArtifact& InArtifact, std::string_view InBlock)
{
	const auto Found = std::find_if(InArtifact.Bindings.begin(), InArtifact.Bindings.end(),
	                                [&](const auto& InBinding)
	                                {
		                                return InBinding.Name == InBlock;
	                                });
	HYP_CHECK(Found != InArtifact.Bindings.end());
	auto Binding = *Found;
	HYP_CHECK(NormalizeStandardMaterialBlock(Binding, TestShaderContracts()));
}

void CheckConsumers(FShaderCompiler& InCompiler, EShaderFormat InFormat)
{
	FShaderCompileOptions Options;
	Options.Defines = {{"HYP_FORWARD_HDR", "1"}, {"HYP_ENABLE_INSTANCE", "0"}};
	const auto Forward = InCompiler.Compile("Model.hlsl", "PSMain", EShaderStage::Pixel, InFormat, Options);
	CheckLightingBindings(Forward, "Model.hlsl (HYP_FORWARD_HDR=1)", 15);
	CheckLightingBindings(InCompiler.Compile("Deferred/Clustered.hlsl", "PSMain", EShaderStage::Pixel, InFormat),
	                      "Deferred/Clustered.hlsl", 15);
	// HYP_NO_DIRECTIONAL disables the primary light; additional directional lights still contribute.
	CheckLightingBindings(InCompiler.Compile("Deferred/ClusteredOnly.hlsl", "PSMain", EShaderStage::Pixel, InFormat),
	                      "Deferred/ClusteredOnly.hlsl", 15);
	CheckLightingBindings(InCompiler.Compile("Deferred/Lighting.hlsl", "PSMain", EShaderStage::Pixel, InFormat),
	                      "Deferred/Lighting.hlsl", 8);
	CheckVolumeContract(InCompiler.Compile("Deferred/LocalLight.hlsl", "VSMain", EShaderStage::Vertex, InFormat),
	                    "LocalVolumeV1");
	CheckVolumeContract(InCompiler.Compile("Deferred/LocalLight.hlsl", "PSMain", EShaderStage::Pixel, InFormat),
	                    "LocalLightV1");
}
} // namespace

void CheckLightingWireShaders()
{
	using namespace Hyperion;
	const auto Root = RendererTestShaderRoot();
	std::filesystem::copy_file(std::filesystem::path(HYP_SOURCE_DIR) / "Source/Tests/Shaders/LightingWireReadback.hlsl",
	                           Root / "LightingWireReadback.hlsl", std::filesystem::copy_options::overwrite_existing);
	FShaderCompiler Compiler(Root, "lighting-wire-shader-cache");
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const auto Fixture = Compiler.Compile("LightingWireReadback.hlsl", "CSMain", EShaderStage::Compute, Format);
		CheckLightingBindings(Fixture, "LightingWireReadback.hlsl", 15);
		const auto Cached = Compiler.Compile("LightingWireReadback.hlsl", "CSMain", EShaderStage::Compute, Format);
		HYP_CHECK(Cached.bCacheHit && Cached.Bytes == Fixture.Bytes && Cached.Bindings == Fixture.Bindings &&
		          Cached.Reflection == Fixture.Reflection);
		CheckConsumers(Compiler, Format);
	}
}
