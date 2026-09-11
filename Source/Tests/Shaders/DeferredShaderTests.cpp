#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Support/TestSupport.h"
#include <algorithm>

using namespace Hyperion;

void CheckDeferredShaders()
{
	FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "deferred-shader-contract-cache");
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
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
				                              InBinding.Name == "HyperionSceneV1";
			                       }));
			Options.Defines.front() = {"HYP_FORWARD_HDR", "1"};
			const auto Forward = Compiler.Compile("Model.hlsl", "PSMain", EShaderStage::Pixel, Format, Options);
			HYP_CHECK(Forward.Reflection.Outputs.size() == 1 && Forward.CacheKey != Base.CacheKey);
		}
		for (const auto* Shader : {"Deferred/Lighting.hlsl", "Deferred/Debug.hlsl", "Common/Tonemap.hlsl"})
		{
			const auto Pixel = Compiler.Compile(Shader, "PSMain", EShaderStage::Pixel, Format);
			HYP_CHECK(!Pixel.Bytes.empty() && Pixel.Reflection.Outputs.size() == 1);
		}
	}
}
