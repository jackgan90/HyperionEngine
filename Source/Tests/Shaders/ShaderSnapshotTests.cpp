#include "Hyperion/IO/IOService.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Support/TestSupport.h"
#include <array>
#include <chrono>
#include <map>
#include <string_view>

namespace
{
using namespace Hyperion;

class FCountingShaderFiles final : public IFileSystem
{
public:
	explicit FCountingShaderFiles(std::shared_ptr<IFileSystem> InFiles) : Files(std::move(InFiles))
	{
	}

	FBytes Read(const std::filesystem::path&, std::size_t) override
	{
		++DirectReads;
		throw std::runtime_error("Shader compilation read live bytes outside its snapshot");
	}

	std::vector<FFileContents> ReadTree(const std::filesystem::path& InDirectory,
	                                    std::span<const std::string_view> InExtensions, std::size_t InLimit) override
	{
		++TreeReads[Normalize(InDirectory)];
		return Files->ReadTree(InDirectory, InExtensions, InLimit);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		Files->WriteAtomic(InPath, InBytes);
	}

	std::filesystem::path Normalize(const std::filesystem::path& InPath) const override
	{
		return Files->Normalize(InPath);
	}

	std::map<std::filesystem::path, std::size_t> TreeReads;
	std::size_t DirectReads{};

private:
	std::shared_ptr<IFileSystem> Files;
};

void WriteText(IFileSystem& InFiles, const std::filesystem::path& InPath, std::string_view InText)
{
	InFiles.WriteAtomic(InPath, std::as_bytes(std::span(InText)));
}

template<typename TAction> void Rejects(TAction InAction)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

constexpr std::string_view LocalSource =
    "#include \"Shared.fragment\"\n"
    "#ifndef TEST_SCALE\n#define TEST_SCALE 1\n#endif\n"
    "float4 VSMain(uint Id : SV_VertexID) : SV_Position { return float4(float(Id), Value, 0, 1); }\n"
    "float4 PSMain() : SV_Target { return float4(Value * TEST_SCALE, 0, 0, 1); }\n";
constexpr std::string_view OriginalInclude = "static const float Value = 0.25;\n";
constexpr std::string_view ChangedInclude = "static const float Value = 0.75;\n";

void CheckFrozenStages(const std::filesystem::path& InFolder)
{
	const auto Root = InFolder / "Source";
	auto Files = std::make_shared<FCountingShaderFiles>(std::make_shared<FLocalFileSystem>());
	WriteText(*Files, Root / "Main.hlsl", LocalSource);
	WriteText(*Files, Root / "Shared.fragment", OriginalInclude);
	FShaderCompiler Compiler(Root, InFolder / "Cache", Files);
	const std::array Paths{std::filesystem::path("Main.hlsl"), std::filesystem::path("Main.hlsl")};
	const auto Snapshot = Compiler.CaptureSources(Paths);
	HYP_CHECK(Files->TreeReads[Root] == 1);
	WriteText(*Files, Root / "Shared.fragment", ChangedInclude);
	std::vector<FShaderArtifact> Frozen;
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		Frozen.push_back(Compiler.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, Format, {}, Snapshot));
		HYP_CHECK(!Frozen.back().bCacheHit && !Frozen.back().Bytes.empty());
		const auto Vertex = Compiler.Compile("Main.hlsl", "VSMain", EShaderStage::Vertex, Format, {}, Snapshot);
		const auto Variant =
		    Compiler.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, Format, {{{"TEST_SCALE", "2"}}}, Snapshot);
		HYP_CHECK(!Vertex.Bytes.empty() && Variant.CacheKey != Frozen.back().CacheKey);
	}
	HYP_CHECK(Files->TreeReads[Root] == 1 && Files->DirectReads == 0);
	// Compile the original bytes into a separate cache, so a key/payload mismatch cannot hide behind a cache hit.
	WriteText(*Files, Root / "Shared.fragment", OriginalInclude);
	FShaderCompiler Reference(Root, InFolder / "ReferenceCache", Files);
	for (const auto& Expected : Frozen)
	{
		const auto Reads = Files->TreeReads[Root];
		const auto Actual = Reference.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, Expected.Format);
		HYP_CHECK(Files->TreeReads[Root] == Reads + 1);
		HYP_CHECK(Actual.CacheKey == Expected.CacheKey && Actual.Bytes == Expected.Bytes);
		HYP_CHECK(Actual.Reflection == Expected.Reflection && Actual.Bindings == Expected.Bindings);
	}
	WriteText(*Files, Root / "Shared.fragment", ChangedInclude);
	for (const auto& Old : Frozen)
	{
		const auto Reads = Files->TreeReads[Root];
		const auto Changed = Compiler.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, Old.Format);
		HYP_CHECK(Files->TreeReads[Root] == Reads + 1);
		HYP_CHECK(Changed.CacheKey != Old.CacheKey && Changed.Bytes != Old.Bytes);
	}
	Rejects(
	    [&]
	    {
		    Reference.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, {}, Snapshot);
	    });
	Rejects(
	    [&]
	    {
		    Compiler.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, {}, {});
	    });
	HYP_CHECK(Files->DirectReads == 0);
}

void CheckFileSetChanges(const std::filesystem::path& InFolder)
{
	const auto Root = InFolder / "Source";
	auto Files = std::make_shared<FCountingShaderFiles>(std::make_shared<FLocalFileSystem>());
	WriteText(*Files, Root / "Main.hlsl", LocalSource);
	WriteText(*Files, Root / "Shared.fragment", OriginalInclude);
	FShaderCompiler Compiler(Root, InFolder / "Cache", Files);
	const std::array Paths{std::filesystem::path("Main.hlsl")};
	const auto Snapshot = Compiler.CaptureSources(Paths);
	const auto Before = Compiler.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, {}, Snapshot);
	WriteText(*Files, Root / "Extra.arbitrary", "unused source file\n");
	const auto Added = Compiler.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
	HYP_CHECK(Added.CacheKey != Before.CacheKey);
	HYP_CHECK(std::filesystem::remove(Root / "Extra.arbitrary"));
	const auto Removed = Compiler.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
	HYP_CHECK(Removed.CacheKey == Before.CacheKey && Removed.Bytes == Before.Bytes);
	HYP_CHECK(std::filesystem::remove(Root / "Shared.fragment"));
	const auto Reads = Files->TreeReads[Root];
	const auto Retained = Compiler.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Spirv,
	                                       {{{"TEST_SCALE", "3"}}}, Snapshot);
	HYP_CHECK(!Retained.Bytes.empty() && Files->TreeReads[Root] == Reads);
	Rejects(
	    [&]
	    {
		    Compiler.Compile("Main.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
	    });
	WriteText(*Files, Root / "Later.hlsl", "float4 PSMain() : SV_Target { return float4(0,1,0,1); }\n");
	Rejects(
	    [&]
	    {
		    Compiler.Compile("Later.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, {}, Snapshot);
	    });
	const auto Later = Compiler.Compile("Later.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
	HYP_CHECK(!Later.Bytes.empty() && Files->DirectReads == 0);
}

void CheckIndependentMountRoots(const std::filesystem::path& InFolder)
{
	FLocalFileSystem Local;
	WriteText(Local, InFolder / "Engine/Shaders/Shared.inc", OriginalInclude);
	WriteText(Local, InFolder / "Game/Shaders/Main.hlsl",
	          "#include \"/Engine/Shaders/Shared.inc\"\nfloat4 PSMain() : SV_Target { return Value; }\n");
	WriteText(Local, InFolder / "Other/Shaders/Main.hlsl", "float4 PSMain() : SV_Target { return 0; }\n");
	auto Mounted =
	    std::make_shared<FMountedFileSystem>(std::vector<FContentMount>{{"/Engine", InFolder / "Engine", false},
	                                                                    {"/Game", InFolder / "Game", false},
	                                                                    {"/Other", InFolder / "Other", false}});
	auto Files = std::make_shared<FCountingShaderFiles>(Mounted);
	FShaderCompiler Compiler("/Engine/Shaders", InFolder / "Cache", Files);
	const std::array Paths{std::filesystem::path("/Game/Shaders/Main.hlsl"),
	                       std::filesystem::path("/Other/Shaders/Main.hlsl"),
	                       std::filesystem::path("/Game/Shaders/Main.hlsl")};
	const auto Snapshot = Compiler.CaptureSources(Paths);
	for (const auto* Root : {"/Engine/Shaders", "/Game/Shaders", "/Other/Shaders"})
	{
		HYP_CHECK(Files->TreeReads[Root] == 1);
	}
	const auto Batch = Compiler.Compile(Paths[0], "PSMain", EShaderStage::Pixel, EShaderFormat::Spirv, {}, Snapshot);
	const auto Other = Compiler.Compile(Paths[1], "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, {}, Snapshot);
	const auto Single = Compiler.Compile(Paths[0], "PSMain", EShaderStage::Pixel, EShaderFormat::Spirv);
	HYP_CHECK(Single.CacheKey == Batch.CacheKey && Single.Bytes == Batch.Bytes && Single.bCacheHit);
	HYP_CHECK(Files->TreeReads["/Engine/Shaders"] == 2 && Files->TreeReads["/Game/Shaders"] == 2 &&
	          Files->TreeReads["/Other/Shaders"] == 1);
	WriteText(*Files, Paths[1], "float4 PSMain() : SV_Target { return 1; }\n");
	const auto NewSnapshot = Compiler.CaptureSources(Paths);
	const auto Unchanged =
	    Compiler.Compile(Paths[0], "PSMain", EShaderStage::Pixel, EShaderFormat::Spirv, {}, NewSnapshot);
	const auto Changed =
	    Compiler.Compile(Paths[1], "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, {}, NewSnapshot);
	HYP_CHECK(Unchanged.CacheKey == Batch.CacheKey && Changed.CacheKey != Other.CacheKey);
	const auto GameOnly = Compiler.CaptureSources(std::span(Paths).first(1));
	Rejects(
	    [&]
	    {
		    Compiler.Compile(Paths[1], "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, {}, GameOnly);
	    });
	WriteText(*Files, "/Other/Shaders/Forbidden.inc", OriginalInclude);
	WriteText(*Files, Paths[0],
	          "#include \"/Other/Shaders/Forbidden.inc\"\nfloat4 PSMain() : SV_Target { return Value; }\n");
	const auto Forbidden = Compiler.CaptureSources(Paths);
	Rejects(
	    [&]
	    {
		    Compiler.Compile(Paths[0], "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil, {}, Forbidden);
	    });
	HYP_CHECK(Files->DirectReads == 0);
}
} // namespace

void TestShaderSnapshots()
{
	const auto Root = std::filesystem::absolute("shader-test/snapshots") /
	                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	CheckFrozenStages(Root / "Frozen");
	CheckFileSetChanges(Root / "Files");
	CheckIndependentMountRoots(Root / "Mounted");
}
