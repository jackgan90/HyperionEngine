#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Support/LogSupport.h"
#include "Support/ShaderSourceSupport.h"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

void CheckDeferredShaders();
void CheckComputeShaders(const std::filesystem::path& InRoot);
void TestMountedShaders();
void TestShaderSnapshots();
void CheckMaterialShaderReflection(const std::filesystem::path& InRoot);

namespace Hyperion::ShadersPrivate
{
void CheckShaderCompilationContracts();
}

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

void CheckDiagnosticLogging()
{
	const auto Root = std::filesystem::absolute("shader-diagnostics") / std::to_string(ClockNanoseconds());
	FTestLogCapture Logs(Root);
	std::filesystem::create_directories(Root / "Source");
	{
		std::ofstream Source(Root / "Source/Warning.hlsl");
		Source << "float4 PSMain() : SV_Target { float2 Value = float3(1, 2, 3); return float4(Value, 0, 1); }";
	}
	FShaderCompiler Compiler(Root / "Source", Root / "Cache");
	Compiler.Compile("Warning.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
	Check(Logs.Count(ELogLevel::Warning,
	                 {"Shader compiler diagnostics;", "Warning.hlsl", "PSMain", "profile=ps_6_0", "target=DXIL"}) == 1,
	      "Successful compiler warnings include context");
	const auto Before = Logs.History->Count();
	Check(Compiler.Compile("Warning.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil).bCacheHit,
	      "Diagnostic shader cache hit");
	Check(Logs.History->Count() == Before, "Cache hits do not replay compiler diagnostics");
}

void CheckCompileOptionDomains()
{
	const auto Root = std::filesystem::absolute("shader-option-domains") / std::to_string(ClockNanoseconds());
	std::filesystem::create_directories(Root / "Source");
	std::ofstream(Root / "Source/Options.hlsl") << R"(
float4 PSMain() : SV_Target0
{
#ifdef FLAG
    return 1;
#else
    return 0;
#endif
}
)";
	FShaderCompiler Compiler(Root / "Source", Root / "Cache");
	FShaderCompiler Independent(Root / "Source", Root / "IndependentCache");
	FShaderCompileOptions Defined;
	Defined.Defines = {{"FLAG", "1"}};
	FShaderCompileOptions Included;
	Included.VirtualIncludes = {{"FLAG", "1"}};
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const auto First = Compiler.Compile("Options.hlsl", "PSMain", EShaderStage::Pixel, Format, Defined);
		const auto Second = Compiler.Compile("Options.hlsl", "PSMain", EShaderStage::Pixel, Format, Included);
		const auto Expected = Independent.Compile("Options.hlsl", "PSMain", EShaderStage::Pixel, Format, Included);
		Check(!First.bCacheHit && !Second.bCacheHit && First.CacheKey != Second.CacheKey,
		      "Compile option domain boundaries");
		Check(First.Bytes != Second.Bytes && Second.Bytes == Expected.Bytes,
		      "Cold compilation matches separated cache result");
		const auto Warm = Compiler.Compile("Options.hlsl", "PSMain", EShaderStage::Pixel, Format, Included);
		Check(Warm.bCacheHit && Warm.Bytes == Expected.Bytes, "Warm cache preserves option domain identity");
	}
}

void CheckCachedIntermediate(const std::filesystem::path& InCache, const FShaderArtifact& InArtifact)
{
	std::ifstream File(InCache / (InArtifact.CacheKey + ".bin"), std::ios::binary);
	const std::string Cached{std::istreambuf_iterator<char>(File), {}};
	Check(Cached.size() > 69 && Cached[64] == '\n', "Cache stores a checksummed intermediate");
	if (InArtifact.Format != EShaderFormat::Dxil)
	{
		Check(Cached.substr(65, 4) == std::string("\x03\x02\x23\x07", 4), "SPIR-V magic in cached intermediate");
	}
	if (InArtifact.Format == EShaderFormat::Msl)
	{
		const std::string Final(InArtifact.Bytes.begin(), InArtifact.Bytes.end());
		Check(Final.find("metal_stdlib") != std::string::npos && Final != Cached.substr(65),
		      "MSL output is regenerated from its cached SPIR-V payload");
	}
}

void CheckPolicyCacheRecovery()
{
	const auto Root = std::filesystem::absolute("shader-policy-cache") / std::to_string(ClockNanoseconds());
	std::filesystem::create_directories(Root / "Source");
	std::ofstream(Root / "Source/Policy.hlsl") << R"(
cbuffer Parameters : register(b0) { float Value; };
Texture2D<float4> Source : register(t0);
SamplerState LinearSampler : register(s0);
float4 PSMain() : SV_Target0 { return Source.SampleLevel(LinearSampler, float2(Value, .5), 0) * Value; }
)";
	const auto Cache = Root / "Cache";
	FShaderCompiler Compiler(Root / "Source", Cache);
	const std::array Sources{std::filesystem::path("Policy.hlsl")};
	const auto Snapshot = Compiler.CaptureSources(Sources);
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const auto Original = Compiler.Compile("Policy.hlsl", "PSMain", EShaderStage::Pixel, Format, {}, Snapshot);
		Check(!Original.bCacheHit, "New applicable policy starts with a cold cache");
		CheckCachedIntermediate(Cache, Original);
		FShaderCompileOptions ChangedOptions;
		ChangedOptions.bOptimize = false;
		const auto Changed =
		    Compiler.Compile("Policy.hlsl", "PSMain", EShaderStage::Pixel, Format, ChangedOptions, Snapshot);
		Check(!Changed.bCacheHit && Changed.CacheKey != Original.CacheKey,
		      "Actual optimization policy changes artifact identity");
		const auto Restored = Compiler.Compile("Policy.hlsl", "PSMain", EShaderStage::Pixel, Format, {}, Snapshot);
		Check(Restored.bCacheHit && Restored.CacheKey == Original.CacheKey && Restored.Bytes == Original.Bytes &&
		          Restored.Bindings == Original.Bindings && Restored.Reflection == Original.Reflection,
		      "Restoring actual policy reuses identical bytes and reflection");
		{
			std::ofstream Damaged(Cache / (Original.CacheKey + ".bin"), std::ios::binary | std::ios::trunc);
			Damaged << "corrupt";
		}
		const auto Repaired = Compiler.Compile("Policy.hlsl", "PSMain", EShaderStage::Pixel, Format, {}, Snapshot);
		Check(!Repaired.bCacheHit && Repaired.CacheKey == Original.CacheKey && Repaired.Bytes == Original.Bytes &&
		          Repaired.Bindings == Original.Bindings && Repaired.Reflection == Original.Reflection,
		      "Corrupted intermediate is rebuilt with identical bytes and reflection");
		CheckCachedIntermediate(Cache, Repaired);
		const auto Hot = Compiler.Compile("Policy.hlsl", "PSMain", EShaderStage::Pixel, Format, {}, Snapshot);
		Check(Hot.bCacheHit && Hot.Bytes == Repaired.Bytes && Hot.Bindings == Repaired.Bindings &&
		          Hot.Reflection == Repaired.Reflection,
		      "Repaired intermediate supports the complete hot path");
	}
}
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		ShadersPrivate::CheckShaderCompilationContracts();
		TestMountedShaders();
		TestShaderSnapshots();
		CheckDiagnosticLogging();
		CheckCompileOptionDomains();
		CheckPolicyCacheRecovery();
		auto Root = std::filesystem::absolute("shader-test/source");
		std::filesystem::create_directories(Root);
		for (const auto& Include : GenerateShaderIncludes())
		{
			std::filesystem::copy_file(TestShaderRoot() / Include.first, Root / Include.first,
			                           std::filesystem::copy_options::overwrite_existing);
		}
		for (auto Name : {"Triangle.hlsl", "Gui.hlsl", "Common.hlsli", "HyperionUniforms.generated.hlsli"})
		{
			std::filesystem::copy_file(TestShaderRoot() / Name, Root / Name,
			                           std::filesystem::copy_options::overwrite_existing);
		}
		std::filesystem::create_directories(Root / "Common");
		std::filesystem::copy_file(TestShaderRoot() / "Common/ColorSpace.hlsli", Root / "Common/ColorSpace.hlsli",
		                           std::filesystem::copy_options::overwrite_existing);
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
		CheckComputeShaders(Root);
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
