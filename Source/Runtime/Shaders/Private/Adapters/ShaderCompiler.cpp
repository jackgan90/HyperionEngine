#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/IO/IOService.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/IO/Path.h"
#include "ShaderReflection.h"
// DXC's Windows declarations require the COM types provided by WRL first.
#include <Windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bcrypt.h>
#include <cstring>
#include <dxcapi.h>
#include <fstream>
#include <mutex>
#include <stdexcept>

namespace Hyperion
{
using Microsoft::WRL::ComPtr;

namespace
{
void Checked(HRESULT InHr, const char* InOperation)
{
	if (FAILED(InHr))
	{
		throw std::runtime_error(InOperation);
	}
}

std::string Read(const std::filesystem::path& InPath)
{
	std::ifstream Stream(InPath, std::ios::binary);
	if (!Stream)
	{
		throw std::runtime_error("Cannot read shader file: " + InPath.string());
	}
	return {std::istreambuf_iterator<char>(Stream), {}};
}

std::string ReadSource(IFileSystem& InFiles, const std::filesystem::path& InPath)
{
	const auto Bytes = InFiles.Read(InPath, 16u * 1024u * 1024u);
	return {reinterpret_cast<const char*>(Bytes.data()), Bytes.size()};
}

std::filesystem::path PackageRoot(const std::filesystem::path& InPath)
{
	const auto Relative = InPath.relative_path();
	return std::filesystem::path("/") / *Relative.begin();
}

std::string Sha256(const std::string& InBytes)
{
	BCRYPT_ALG_HANDLE Algorithm{};
	BCRYPT_HASH_HANDLE Hash{};
	if (BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
	{
		throw std::runtime_error("SHA256 provider");
	}

	struct FGuard
	{
		BCRYPT_ALG_HANDLE A;
		BCRYPT_HASH_HANDLE& H;

		~FGuard()
		{
			if (H)
			{
				BCryptDestroyHash(H);
			}
			BCryptCloseAlgorithmProvider(A, 0);
		}
	} Guard{Algorithm, Hash};

	if (BCryptCreateHash(Algorithm, &Hash, nullptr, 0, nullptr, 0, 0) < 0)
	{
		throw std::runtime_error("SHA256 initialization");
	}
	std::size_t Offset{};
	while (Offset < InBytes.size())
	{
		auto Size = static_cast<ULONG>(std::min<std::size_t>(InBytes.size() - Offset, 1024 * 1024));
		if (BCryptHashData(Hash, reinterpret_cast<PUCHAR>(const_cast<char*>(InBytes.data() + Offset)), Size, 0) < 0)
		{
			throw std::runtime_error("SHA256 update");
		}
		Offset += Size;
	}
	unsigned char Digest[32]{};
	if (BCryptFinishHash(Hash, Digest, 32, 0) < 0)
	{
		throw std::runtime_error("SHA256 finish");
	}
	const char* Hex = "0123456789abcdef";
	std::string Result;
	for (auto B : Digest)
	{
		Result += Hex[B >> 4];
		Result += Hex[B & 15];
	}
	return Result;
}

bool Within(const std::filesystem::path& InPath, const std::filesystem::path& InRoot)
{
	auto Rel = InPath.lexically_relative(InRoot);
	return !Rel.empty() && !Rel.is_absolute() && *Rel.begin() != "..";
}

class FIncludeHandler final : public IDxcIncludeHandler
{
public:
	FIncludeHandler(IDxcUtils* InUtils, std::filesystem::path InRoot, IFileSystem& InFiles,
	                std::filesystem::path InAdditionalRoot, std::string& OutError)
	    : Utils(InUtils), Root(std::move(InRoot)), Files(InFiles), AdditionalRoot(std::move(InAdditionalRoot)),
	      Error(OutError)
	{
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID InId, void** OutObject) override
	{
		if (!OutObject)
		{
			return E_POINTER;
		}
		*OutObject = nullptr;
		if (InId == __uuidof(IUnknown) || InId == __uuidof(IDxcIncludeHandler))
		{
			*OutObject = static_cast<IDxcIncludeHandler*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++Refs;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		auto N = --Refs;
		if (!N)
		{
			delete this;
		}
		return N;
	}

	HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR InName, IDxcBlob** OutSource) override
	{
		if (!OutSource)
		{
			return E_POINTER;
		}
		*OutSource = nullptr;
		try
		{
			auto Path = std::filesystem::path(InName);
			if (IsPackagePath(Root) && !IsPackagePath(Path))
			{
				// DXC spells rooted virtual includes as ./Mount/... on Windows.
				const auto Text = PathToUtf8(Path.lexically_normal());
				for (const auto& Allowed : {PackageRoot(Root), AdditionalRoot})
				{
					if (!Allowed.empty() && Text.starts_with(PathToUtf8(Allowed.relative_path()) + "/"))
					{
						Path = PathFromUtf8("/" + Text);
						break;
					}
				}
			}
			if (Path.is_relative() && !IsPackagePath(Path))
			{
				Path = Root / Path;
			}
			Path = Files.Normalize(Path);
			if (!IsPackagePath(Path))
			{
				Path = std::filesystem::weakly_canonical(Path);
			}
			if (!Within(Path, Root) && (AdditionalRoot.empty() || !Within(Path, AdditionalRoot)))
			{
				Error += "\nInclude outside shader roots: " + PathToUtf8(Path);
				return E_ACCESSDENIED;
			}
			ComPtr<IDxcBlobEncoding> Blob;
			const auto Content = ReadSource(Files, Path);
			auto Hr = Utils->CreateBlob(Content.data(), static_cast<UINT32>(Content.size()), DXC_CP_UTF8, &Blob);
			if (FAILED(Hr))
			{
				return Hr;
			}
			*OutSource = Blob.Detach();
			return S_OK;
		}
		catch (const std::exception& Failure)
		{
			Error += "\n" + PathToUtf8(std::filesystem::path(InName)) + ": " + Failure.what();
			return E_FAIL;
		}
	}

private:
	std::atomic<ULONG> Refs{1};
	ComPtr<IDxcUtils> Utils;
	std::filesystem::path Root;
	IFileSystem& Files;
	std::filesystem::path AdditionalRoot;
	std::string& Error;
};
} // namespace

struct FShaderCompiler::FImpl
{
	std::shared_ptr<IFileSystem> Files;
	std::filesystem::path Root;
	std::filesystem::path Cache;
	std::mutex Mutex;
	ComPtr<IDxcUtils> Utils;
	ComPtr<IDxcCompiler3> Compiler;

	FImpl(std::filesystem::path InR, std::filesystem::path InC, std::shared_ptr<IFileSystem> InFiles)
	    : Files(std::move(InFiles)), Root(Files->Normalize(InR)), Cache(std::filesystem::absolute(InC))
	{
		if (const auto Mounted = std::dynamic_pointer_cast<FMountedFileSystem>(Files))
		{
			Cache = Mounted->Resolve(Cache, true);
		}
		const auto CachePath = Files->Normalize(Cache);
		if (Within(CachePath, Root) ||
		    (IsPackagePath(CachePath) && Within(CachePath, PackageRoot(CachePath) / "Shaders")))
		{
			throw std::invalid_argument("Shader cache must be outside source root");
		}
		std::filesystem::create_directories(Cache);
		Checked(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils)), "Create DXC utils");
		Checked(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)), "Create DXC compiler");
	}
};

FShaderCompiler::FShaderCompiler(std::filesystem::path InRoot, std::filesystem::path InCache)
    : FShaderCompiler(std::move(InRoot), std::move(InCache), std::make_shared<FLocalFileSystem>())
{
}

FShaderCompiler::FShaderCompiler(std::filesystem::path InRoot, std::filesystem::path InCache,
                                 std::shared_ptr<IFileSystem> InFiles)
{
	if (!InFiles)
	{
		throw std::invalid_argument("Null shader filesystem");
	}
	Impl = std::make_unique<FImpl>(std::move(InRoot), std::move(InCache), std::move(InFiles));
}

FShaderCompiler::~FShaderCompiler() = default;

namespace
{
std::string ShaderCacheKey(const std::filesystem::path& InPath, const std::filesystem::path& InRoot,
                           const std::string& InEntry, EShaderStage InStage, EShaderFormat InFormat,
                           const FShaderCompileOptions& InOptions, IFileSystem& InFiles)
{
	std::string Identity = "hyperion-shader-v9-mounted-msl20:" HYP_TOOLCHAIN_ID;
	auto Append = [&](const std::string& InPart)
	{
		Identity += std::to_string(InPart.size()) + ":" + InPart;
	};
	Append(InPath.lexically_relative(InRoot).generic_string());
	Append(InEntry);
	Append(std::to_string(static_cast<int>(InStage)));
	Append(std::to_string(static_cast<int>(InFormat)));
	Append(std::to_string(ShaderBindingMappingVersion));
	Append(std::to_string(FShaderReflection{}.Version));
	Append(InOptions.bOptimize ? "O3" : "Od");
	for (const FShaderDefine& Define : InOptions.Defines)
	{
		Append(Define.Name);
		Append(Define.Value);
	}
	// DXC accepts include files with arbitrary extensions; every source-root file participates.
	auto Files = InFiles.ReadTree(InRoot, {}, 16u * 1024u * 1024u);
	if (IsPackagePath(InPath) && !Within(InPath, InRoot))
	{
		for (auto& File : InFiles.ReadTree(PackageRoot(InPath) / "Shaders", {}, 16u * 1024u * 1024u))
		{
			Files.push_back(std::move(File));
		}
	}
	std::sort(Files.begin(), Files.end(),
	          [](const FFileContents& InA, const FFileContents& InB)
	          {
		          return InA.Path < InB.Path;
	          });
	for (const auto& File : Files)
	{
		Append(File.Path.lexically_relative(InRoot).generic_string());
		Append({reinterpret_cast<const char*>(File.Bytes.data()), File.Bytes.size()});
	}
	return Sha256(Identity);
}

void NormalizeOptions(FShaderCompileOptions& InOptions)
{
	std::sort(InOptions.Defines.begin(), InOptions.Defines.end(),
	          [](const FShaderDefine& InA, const FShaderDefine& InB)
	          {
		          return InA.Name < InB.Name;
	          });
	std::string Previous;
	for (const FShaderDefine& Define : InOptions.Defines)
	{
		if (Define.Name.empty() || Define.Name == Previous ||
		    Define.Name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
		        std::string::npos ||
		    (Define.Name.front() >= '0' && Define.Name.front() <= '9') ||
		    Define.Value.find_first_of("\r\n") != std::string::npos)
		{
			throw std::invalid_argument("Invalid or duplicate shader define: " + Define.Name);
		}
		Previous = Define.Name;
	}
}

std::string CompilePayload(IDxcCompiler3* InCompiler, IDxcUtils* InUtils, const std::filesystem::path& InPath,
                           const std::filesystem::path& InRoot, const std::string& InEntry, EShaderStage InStage,
                           EShaderFormat InFormat, const FShaderCompileOptions& InOptions, IFileSystem& InFiles)
{
	const std::string Content = ReadSource(InFiles, InPath);
	DxcBuffer Buffer{Content.data(), Content.size(), DXC_CP_UTF8};
	std::vector<std::wstring> Arguments{InPath.generic_wstring(),
	                                    L"-E",
	                                    std::wstring(InEntry.begin(), InEntry.end()),
	                                    L"-T",
	                                    InStage == EShaderStage::Vertex ? L"vs_6_0" : L"ps_6_0",
	                                    L"-HV",
	                                    L"2021",
	                                    L"-Ges",
	                                    InOptions.bOptimize ? L"-O3" : L"-Od",
	                                    L"-I",
	                                    InPath.parent_path().generic_wstring(),
	                                    L"-I",
	                                    InRoot.generic_wstring()};
	if (IsPackagePath(InRoot))
	{
		Arguments.insert(Arguments.end(), {L"-I", L"/"});
	}
	for (const FShaderDefine& Define : InOptions.Defines)
	{
		const std::string Argument = "-D" + Define.Name + "=" + Define.Value;
		Arguments.emplace_back(Argument.begin(), Argument.end());
	}
	if (InFormat != EShaderFormat::Dxil)
	{
		Arguments.insert(Arguments.end(), {L"-spirv", L"-fspv-target-env=vulkan1.1", L"-fspv-reflect"});
		for (std::uint32_t Space = 0; Space < ShaderRegisterSpaceCount; ++Space)
		{
			for (std::uint32_t Kind = 0; Kind < 4; ++Kind)
			{
				const std::array<std::wstring, 4> Shifts{L"-fvk-b-shift", L"-fvk-t-shift", L"-fvk-s-shift",
				                                         L"-fvk-u-shift"};
				Arguments.insert(Arguments.end(), {Shifts[Kind], std::to_wstring(Kind * ShaderRegistersPerKind),
				                                   std::to_wstring(Space)});
			}
		}
	}
	std::vector<LPCWSTR> Pointers;
	for (const std::wstring& Argument : Arguments)
	{
		Pointers.push_back(Argument.c_str());
	}
	ComPtr<IDxcIncludeHandler> Include;
	std::string IncludeError;
	Include.Attach(new FIncludeHandler(
	    InUtils, InRoot, InFiles, IsPackagePath(InPath) ? PackageRoot(InPath) / "Shaders" : std::filesystem::path{},
	    IncludeError));
	ComPtr<IDxcResult> Result;
	Checked(InCompiler->Compile(&Buffer, Pointers.data(), static_cast<UINT32>(Pointers.size()), Include.Get(),
	                            IID_PPV_ARGS(&Result)),
	        "DXC invocation failed");
	HRESULT Status{};
	Checked(Result->GetStatus(&Status), "DXC status failed");
	if (FAILED(Status))
	{
		ComPtr<IDxcBlobUtf8> Errors;
		Result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&Errors), nullptr);
		throw std::runtime_error(std::string(Errors ? Errors->GetStringPointer() : "Shader compilation failed") + "\n" +
		                         IncludeError);
	}
	ComPtr<IDxcBlob> Object;
	Checked(Result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&Object), nullptr), "DXC object missing");
	return {static_cast<const char*>(Object->GetBufferPointer()), Object->GetBufferSize()};
}

} // namespace

FShaderArtifact FShaderCompiler::Compile(const std::filesystem::path& InSource, std::string InEntry,
                                         EShaderStage InStage, EShaderFormat InFormat)
{
	return Compile(InSource, std::move(InEntry), InStage, InFormat, {});
}

FShaderArtifact FShaderCompiler::Compile(const std::filesystem::path& InSource, std::string InEntry,
                                         EShaderStage InStage, EShaderFormat InFormat, FShaderCompileOptions InOptions)
{
	NormalizeOptions(InOptions);
	if (InEntry.empty() || (InStage != EShaderStage::Vertex && InStage != EShaderStage::Pixel) ||
	    (InFormat != EShaderFormat::Dxil && InFormat != EShaderFormat::Spirv && InFormat != EShaderFormat::Msl))
	{
		throw std::invalid_argument("Unsupported shader entry, stage or target");
	}
	// Logical HLSL types can be lowered (notably bool -> uint) by SPIR-V. Preserve their DXIL source types,
	// while reflecting every offset and stride from the actual SPIR-V intermediate.
	FShaderArtifact Logical{};
	if (InFormat != EShaderFormat::Dxil)
	{
		Logical = Compile(InSource, InEntry, InStage, EShaderFormat::Dxil, InOptions);
	}
	std::lock_guard Lock(Impl->Mutex);
	HYP_PERF_SCOPE_C(Assets, ShaderCompilation);
	auto Path =
	    Impl->Files->Normalize(InSource.is_absolute() || IsPackagePath(InSource) ? InSource : Impl->Root / InSource);
	if (!IsPackagePath(Path))
	{
		Path = std::filesystem::weakly_canonical(Path);
	}
	const bool bMountedShader =
	    IsPackagePath(Path) && IsPackagePath(Impl->Root) && Within(Path, PackageRoot(Path) / "Shaders");
	if (!Within(Path, Impl->Root) && !bMountedShader)
	{
		throw std::invalid_argument("Shader outside source root");
	}
	FShaderArtifact Artifact{};
	Artifact.Format = InFormat;
	Artifact.Stage = InStage;
	Artifact.CacheKey = ShaderCacheKey(Path, Impl->Root, InEntry, InStage, InFormat, InOptions, *Impl->Files);
	const auto CacheFile = Impl->Cache / (Artifact.CacheKey + ".bin");
	std::string Payload;
	if (std::filesystem::exists(CacheFile))
	{
		const std::string Cached = Read(CacheFile);
		if (Cached.size() > 65 && Cached[64] == '\n' && Sha256(Cached.substr(65)) == Cached.substr(0, 64))
		{
			Payload = Cached.substr(65);
			Artifact.bCacheHit = true;
		}
	}
	if (!Artifact.bCacheHit)
	{
		Payload = CompilePayload(Impl->Compiler.Get(), Impl->Utils.Get(), Path, Impl->Root, InEntry, InStage, InFormat,
		                         InOptions, *Impl->Files);
	}
	const std::string Intermediate = Payload;
	if (InFormat == EShaderFormat::Dxil)
	{
		ShadersPrivate::ReflectDxil(Artifact, Payload);
	}
	else
	{
		ShadersPrivate::ReflectSpirv(Artifact, Payload, Logical);
	}
	ShadersPrivate::ValidateShaderBindings(Artifact);
	if (!Artifact.bCacheHit)
	{
		// Cache the unstripped intermediate, so hot and cold paths reconstruct identical reflection.
		std::ofstream Out(CacheFile, std::ios::binary | std::ios::trunc);
		Out << Sha256(Intermediate) << '\n';
		Out.write(Intermediate.data(), static_cast<std::streamsize>(Intermediate.size()));
		if (!Out)
		{
			throw std::runtime_error("Shader cache write failed");
		}
	}
	Artifact.Bytes.assign(Payload.begin(), Payload.end());
	return Artifact;
}
} // namespace Hyperion
