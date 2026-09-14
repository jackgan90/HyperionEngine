#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Support/TestSupport.h"

void TestMountedShaders()
{
	using namespace Hyperion;
	const auto Root = std::filesystem::absolute("shader-test/mounted");
	FLocalFileSystem Local;
	const auto Write = [&](const std::filesystem::path& InPath, std::string_view InText)
	{
		Local.WriteAtomic(InPath, std::as_bytes(std::span(InText)));
	};
	const std::string Source = "#include \"/Engine/Shaders/Shared.inc\"\n#include \"Gain.HLSLI\"\n"
	                           "float4 PSMain() : SV_Target { return Tint * Gain; }\n";
	std::string OriginalKey;
	for (const auto& Location : {"First", "Relocated"})
	{
		const auto Folder = Root / Location;
		Write(Folder / "Engine/Shaders/Shared.inc", "static const float4 Tint = float4(1,0,0,1);\n");
		Write(Folder / "Game/Shaders/Gain.HLSLI", "static const float Gain = 1;\n");
		Write(Folder / "Game/Shaders/Main.hlsl", Source);
		auto Files = std::make_shared<FMountedFileSystem>(
		    std::vector<FContentMount>{{"/Engine", Folder / "Engine", true}, {"/Game", Folder / "Game", false}});
		for (const auto* Mount : {"Engine", "Game"})
		{
			for (const auto* Tail : {"Shaders/RejectedCache", "shaders/RejectedCache"})
			{
				const auto Cache = Folder / Mount / Tail;
				bool bRejected = false;
				try
				{
					FShaderCompiler Invalid("/Engine/Shaders", Cache, Files);
				}
				catch (const std::exception&)
				{
					bRejected = true;
				}
				HYP_CHECK(bRejected && !std::filesystem::exists(Cache));
			}
		}
		FShaderCompiler Compiler("/Engine/Shaders", Root / "Cache", Files);
		for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
		{
			const auto Cold = Compiler.Compile("/Game/Shaders/Main.hlsl", "PSMain", EShaderStage::Pixel, Format);
			const auto Warm = Compiler.Compile("/Game/Shaders/Main.hlsl", "PSMain", EShaderStage::Pixel, Format);
			HYP_CHECK(!Cold.Bytes.empty() && Warm.bCacheHit && Warm.Bytes == Cold.Bytes);
			if (Format == EShaderFormat::Dxil)
			{
				if (OriginalKey.empty())
				{
					OriginalKey = Cold.CacheKey;
				}
				else
				{
					HYP_CHECK(Cold.bCacheHit && Cold.CacheKey == OriginalKey);
				}
			}
		}
		const auto Before =
		    Compiler.Compile("/Game/Shaders/Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
		Write(Folder / "Engine/Shaders/Shared.inc", "static const float4 Tint = float4(0,1,0,1);\n");
		const auto EngineChanged =
		    Compiler.Compile("/Game/Shaders/Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
		HYP_CHECK(EngineChanged.CacheKey != Before.CacheKey && EngineChanged.Bytes != Before.Bytes);
		Write(Folder / "Game/Shaders/Gain.HLSLI", "static const float Gain = .5;\n");
		const auto GameChanged =
		    Compiler.Compile("/Game/Shaders/Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
		HYP_CHECK(GameChanged.CacheKey != EngineChanged.CacheKey && GameChanged.Bytes != EngineChanged.Bytes);
	}
}
