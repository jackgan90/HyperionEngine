#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Support/RendererShaderSupport.h"
#include "Support/TestSupport.h"
#include <algorithm>

using namespace Hyperion;

namespace
{
FShaderCompileOptions DebugOptions(
    std::span<const FGBufferVisualizerOption> InPresentation = GBufferVisualizerOptions())
{
	FShaderCompileOptions Result;
	for (const auto& Define : MakeGBufferVisualizerShaderDefines(InPresentation))
	{
		Result.Defines.push_back({Define.Name, Define.Value});
	}
	return Result;
}

void CheckDebugShader(FShaderCompiler& InCompiler, EShaderFormat InFormat)
{
	const auto Options = DebugOptions();
	HYP_CHECK(Options.Defines.size() == 7);
	for (std::size_t Index = 0; Index < Options.Defines.size(); ++Index)
	{
		HYP_CHECK(Options.Defines[Index].Value == std::to_string(Index));
	}
	const auto Compile = [&](const FShaderCompileOptions& InOptions)
	{
		return InCompiler.Compile("Deferred/Debug.hlsl", "PSMain", EShaderStage::Pixel, InFormat, InOptions);
	};
	const auto Shader = Compile(Options);
	HYP_CHECK(!Shader.Bytes.empty() && Shader.Reflection.Outputs.size() == 1);
	const auto Block = std::find_if(Shader.Bindings.begin(), Shader.Bindings.end(),
	                                [](const auto& InBinding)
	                                {
		                                return InBinding.Name == "GBufferDebugV1";
	                                });
	HYP_CHECK(Block != Shader.Bindings.end() && Block->ByteSize == 16);
	HYP_CHECK(Block->Members.size() == 1 && Block->Members.front().Name == "GBufferDebug");
	const auto& Mode = Block->Members.front().Members.front();
	HYP_CHECK(Mode.Name == "Mode" && Mode.Offset == 0 && Mode.Scalar == EShaderScalar::Uint);
	std::vector<FGBufferVisualizerOption> Reordered(GBufferVisualizerOptions().begin(),
	                                                GBufferVisualizerOptions().end());
	std::reverse(Reordered.begin(), Reordered.end());
	Reordered.front().Label = "Presentation-only rename";
	const auto Presented = DebugOptions(Reordered);
	HYP_CHECK(Presented == Options);
	const auto Same = Compile(Presented);
	HYP_CHECK(Same.bCacheHit && Same.CacheKey == Shader.CacheKey);
	auto Changed = Options;
	Changed.Defines[2].Value = "27";
	const auto Different = Compile(Changed);
	HYP_CHECK(Different.CacheKey != Shader.CacheKey);
	const auto Restored = Compile(Options);
	HYP_CHECK(Restored.bCacheHit && Restored.CacheKey == Shader.CacheKey);
	for (std::size_t Missing = 0; Missing < Options.Defines.size(); ++Missing)
	{
		auto Incomplete = Options;
		Incomplete.Defines.erase(Incomplete.Defines.begin() + Missing);
		bool bRejected{};
		try
		{
			(void)Compile(Incomplete);
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
	}
}
} // namespace

void CheckDeferredShaders()
{
	FShaderCompiler Compiler(RendererTestShaderRoot(), "deferred-shader-contract-cache");
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		CheckDebugShader(Compiler, Format);
		for (const bool bInstance : {false, true})
		{
			FShaderCompileOptions Options;
			Options.Defines = {{"HYP_DEFERRED_BASE", "1"}, {"HYP_ENABLE_INSTANCE", bInstance ? "1" : "0"}};
			const auto Vertex = Compiler.Compile("Model.hlsl", "VSMain", EShaderStage::Vertex, Format, Options);
			const auto Base = Compiler.Compile("Model.hlsl", "PSMain", EShaderStage::Pixel, Format, Options);
			HYP_CHECK(!Vertex.Bytes.empty() && Base.Reflection.Outputs.size() == 4);
			HYP_CHECK(std::none_of(Base.Bindings.begin(), Base.Bindings.end(),
			                       [](const FShaderBinding& InBinding)
			                       {
				                       return InBinding.Name.starts_with("Shadow") ||
				                              InBinding.Name.starts_with("Cluster") ||
				                              InBinding.Name == "HyperionSceneV1";
			                       }));
			Options.Defines.front() = {"HYP_FORWARD_HDR", "1"};
			const auto Forward = Compiler.Compile("Model.hlsl", "PSMain", EShaderStage::Pixel, Format, Options);
			HYP_CHECK(Forward.Reflection.Outputs.size() == 1 && Forward.CacheKey != Base.CacheKey);
			HYP_CHECK(std::any_of(Forward.Bindings.begin(), Forward.Bindings.end(),
			                      [](const FShaderBinding& InBinding)
			                      {
				                      return InBinding.Name == "ClusterLights" &&
				                             InBinding.Kind == EBindingKind::StructuredBuffer &&
				                             InBinding.StructureByteStride == 64;
			                      }));
		}
		const auto Volume = Compiler.Compile("Deferred/LocalLight.hlsl", "VSMain", EShaderStage::Vertex, Format);
		const auto Sky = Compiler.Compile("Common/Sky.hlsl", "PSMain", EShaderStage::Pixel, Format);
		HYP_CHECK(std::any_of(Sky.Bindings.begin(), Sky.Bindings.end(),
		                      [](const auto& InBinding)
		                      {
			                      return InBinding.Name == "SkyRadiance" &&
			                             InBinding.Dimension == EShaderResourceDimension::TextureCube;
		                      }));
		for (const auto* Reversed : {"0", "1"})
		{
			FShaderCompileOptions Options;
			Options.Defines = {{"HYP_REVERSED_SKY", Reversed}};
			HYP_CHECK(
			    !Compiler.Compile("Common/Sky.hlsl", "VSMain", EShaderStage::Vertex, Format, Options).Bytes.empty());
		}
		HYP_CHECK(!Volume.Bytes.empty());
		for (const auto* Shader : {"Deferred/Lighting.hlsl", "Deferred/Clustered.hlsl", "Deferred/ClusteredOnly.hlsl",
		                           "Deferred/LocalLight.hlsl", "Common/Tonemap.hlsl"})
		{
			const auto Pixel = Compiler.Compile(Shader, "PSMain", EShaderStage::Pixel, Format);
			HYP_CHECK(!Pixel.Bytes.empty() && Pixel.Reflection.Outputs.size() == 1);
			if (std::string_view(Shader) == "Deferred/ClusteredOnly.hlsl")
			{
				HYP_CHECK(std::none_of(Pixel.Bindings.begin(), Pixel.Bindings.end(),
				                       [](const auto& InBinding)
				                       {
					                       return InBinding.Name.starts_with("Shadow");
				                       }));
			}
		}
	}
}
