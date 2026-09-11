#include "Hyperion/Shaders/ShaderCompiler.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

void CheckDeferredShaders();
void CheckMaterialShaderReflection(const std::filesystem::path& InRoot);

namespace
{
using namespace Hyperion;

void Check(bool bInB, const char* InM)
{
	if (!bInB)
	{
		throw std::runtime_error(InM);
	}
}
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		auto Root = std::filesystem::absolute("shader-test/source");
		std::filesystem::create_directories(Root);
		for (auto Name : {"Triangle.hlsl", "Gui.hlsl", "Common.hlsli"})
		{
			std::filesystem::copy_file(std::filesystem::path(HYP_SOURCE_DIR) / "shaders" / Name, Root / Name,
			                           std::filesystem::copy_options::overwrite_existing);
		}
		std::filesystem::create_directories(Root / "Common");
		std::filesystem::copy_file(std::filesystem::path(HYP_SOURCE_DIR) / "shaders/Common/ColorSpace.hlsli",
		                           Root / "Common/ColorSpace.hlsli", std::filesystem::copy_options::overwrite_existing);
		FShaderCompiler Compiler(Root, "shader-test/cache");
		for (auto Stage : {EShaderStage::Vertex, EShaderStage::Pixel})
		{
			for (auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
			{
				auto A = Compiler.Compile("Triangle.hlsl", Stage == EShaderStage::Vertex ? "VSMain" : "PSMain", Stage,
				                          Format);
				Check(!A.Bytes.empty(), "Compiled bytes");
				if (Stage == EShaderStage::Vertex && Format != EShaderFormat::Dxil)
				{
					Check(A.Bindings.size() == 1 && A.Bindings[0].Binding == 0 && A.Bindings[0].ByteSize == 64,
					      "Uniform reflection");
				}
				if (Format == EShaderFormat::Msl)
				{
					std::string Text(A.Bytes.begin(), A.Bytes.end());
					Check(Text.find("metal_stdlib") != std::string::npos, "MSL conversion");
				}
			}
		}
		for (auto Stage : {EShaderStage::Vertex, EShaderStage::Pixel})
		{
			for (auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
			{
				auto A =
				    Compiler.Compile("Gui.hlsl", Stage == EShaderStage::Vertex ? "VSMain" : "PSMain", Stage, Format);
				Check(!A.Bytes.empty(), "GUI shader compilation");
				if (Stage == EShaderStage::Pixel && Format != EShaderFormat::Dxil)
				{
					Check(A.Bindings.size() == 2, "Texture and sampler reflection");
					for (const auto& B : A.Bindings)
					{
						Check((B.Kind == EBindingKind::Texture && B.Binding == 1000) ||
						          (B.Kind == EBindingKind::Sampler && B.Binding == 2000),
						      "Non-overlapping Vulkan bindings");
					}
				}
				if (Format == EShaderFormat::Spirv)
				{
					std::ofstream File(Stage == EShaderStage::Vertex ? "shader-test/gui-vs.spv"
					                                                 : "shader-test/gui-ps.spv",
					                   std::ios::binary);
					File.write(reinterpret_cast<const char*>(A.Bytes.data()),
					           static_cast<std::streamsize>(A.Bytes.size()));
				}
			}
		}
		auto First = Compiler.Compile("Triangle.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
		auto Second = Compiler.Compile("Triangle.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
		Check(Second.bCacheHit && First.Bytes == Second.Bytes, "Cache hit");
		{
			std::ofstream File(Root / "Common.hlsli", std::ios::app);
			File << "\n// cache invalidation test\n";
		}
		auto Changed = Compiler.Compile("Triangle.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
		Check(Changed.CacheKey != First.CacheKey, "Transitive include identity");
		{
			std::ofstream File(std::filesystem::path("shader-test/cache") / (Changed.CacheKey + ".bin"));
			File << "corrupt";
		}
		auto Repaired = Compiler.Compile("Triangle.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
		Check(!Repaired.bCacheHit && Repaired.Bytes == Changed.Bytes, "Corrupt cache recovery");
		{
			std::ofstream File(Root / "invalid.hlsl");
			File << "not valid shader source";
		}
		bool bFailed = false;
		try
		{
			Compiler.Compile("invalid.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
		}
		catch (const std::exception& E)
		{
			bFailed = std::string(E.what()).find("error") != std::string::npos;
		}
		Check(bFailed, "Compiler diagnostics");
		CheckMaterialShaderReflection(Root);
		CheckDeferredShaders();
		std::cout << "Shader target, reflection and cache checks passed\n";
		return 0;
	}
	catch (const std::exception& E)
	{
		std::cerr << E.what() << '\n';
		return 1;
	}
}
