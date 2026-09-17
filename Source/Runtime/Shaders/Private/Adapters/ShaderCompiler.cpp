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
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>

namespace Hyperion
{
using Microsoft::WRL::ComPtr;

namespace
{
struct FShaderSourceTree
{
	std::map<std::filesystem::path, std::string> Files;
	std::string Digest;
};

using FShaderSourceTrees = std::map<std::filesystem::path, FShaderSourceTree>;

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

const std::string& ReadSource(const FShaderSourceTrees& InSources, const std::filesystem::path& InRoot,
                              const std::filesystem::path& InAdditionalRoot, const std::filesystem::path& InPath)
{
	for (const auto& Root : {InRoot, InAdditionalRoot})
	{
		const auto Tree = InSources.find(Root);
		if (Tree == InSources.end())
		{
			continue;
		}
		const auto File = Tree->second.Files.find(InPath);
		if (File != Tree->second.Files.end())
		{
			return File->second;
		}
	}
	throw std::runtime_error("Cannot read captured shader file: " + PathToUtf8(InPath));
}

std::filesystem::path PackageRoot(const std::filesystem::path& InPath)
{
	const auto Relative = InPath.relative_path();
	return std::filesystem::path("/") / *Relative.begin();
}

std::filesystem::path AdditionalShaderRoot(const std::filesystem::path& InPath)
{
	return IsPackagePath(InPath) ? PackageRoot(InPath) / "Shaders" : std::filesystem::path{};
}

void AppendIdentity(std::string& OutIdentity, const std::string& InPart)
{
	OutIdentity += std::to_string(InPart.size()) + ":" + InPart;
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

FShaderSourceTree CaptureSourceTree(IFileSystem& InFiles, const std::filesystem::path& InRoot)
{
	FShaderSourceTree Result;
	// DXC accepts arbitrary include extensions; capture every source-root file.
	for (auto& File : InFiles.ReadTree(InRoot, {}, 16u * 1024u * 1024u))
	{
		auto Path = InFiles.Normalize(File.Path);
		if (!IsPackagePath(Path))
		{
			Path = std::filesystem::weakly_canonical(Path);
		}
		if (!Within(Path, InRoot))
		{
			throw std::invalid_argument("Captured shader file outside source root: " + PathToUtf8(Path));
		}
		Result.Files.emplace(std::move(Path),
		                     std::string(reinterpret_cast<const char*>(File.Bytes.data()), File.Bytes.size()));
	}
	std::string Identity;
	for (const auto& [Path, Bytes] : Result.Files)
	{
		AppendIdentity(Identity, Path.lexically_relative(InRoot).generic_string());
		AppendIdentity(Identity, Bytes);
	}
	Result.Digest = Sha256(Identity);
	return Result;
}

class FIncludeHandler final : public IDxcIncludeHandler
{
public:
	FIncludeHandler(IDxcUtils* InUtils, std::filesystem::path InRoot, IFileSystem& InFiles,
	                std::filesystem::path InAdditionalRoot, const FShaderSourceTrees& InSources, std::string& OutError)
	    : Utils(InUtils), Root(std::move(InRoot)), Files(InFiles), AdditionalRoot(std::move(InAdditionalRoot)),
	      Sources(InSources), Error(OutError)
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
			const auto& Content = ReadSource(Sources, Root, AdditionalRoot, Path);
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
	const FShaderSourceTrees& Sources;
	std::string& Error;
};
} // namespace

struct FShaderSourceSnapshot::FImpl
{
	std::shared_ptr<const void> Owner;
	FShaderSourceTrees Trees;
};

struct FShaderCompiler::FImpl
{
	std::shared_ptr<IFileSystem> Files;
	std::filesystem::path Root;
	std::filesystem::path Cache;
	std::mutex Mutex;
	ComPtr<IDxcUtils> Utils;
	ComPtr<IDxcCompiler3> Compiler;
	std::shared_ptr<const void> SnapshotOwner = std::make_shared<const int>(0);

	FShaderArtifact Compile(const std::filesystem::path& InPath, const std::string& InEntry, EShaderStage InStage,
	                        EShaderFormat InFormat, const FShaderCompileOptions& InOptions,
	                        const FShaderSourceTrees& InSources);

	std::filesystem::path SourcePath(const std::filesystem::path& InSource) const
	{
		auto Path = Files->Normalize(InSource.is_absolute() || IsPackagePath(InSource) ? InSource : Root / InSource);
		if (!IsPackagePath(Path))
		{
			Path = std::filesystem::weakly_canonical(Path);
		}
		const bool bMountedShader =
		    IsPackagePath(Path) && IsPackagePath(Root) && Within(Path, AdditionalShaderRoot(Path));
		if (!Within(Path, Root) && !bMountedShader)
		{
			throw std::invalid_argument("Shader outside source root");
		}
		return Path;
	}

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

FShaderSourceSnapshot FShaderCompiler::CaptureSources(std::span<const std::filesystem::path> InSources)
{
	std::lock_guard Lock(Impl->Mutex);
	HYP_PERF_SCOPE_C(Assets, ShaderSourceCapture);
	std::set<std::filesystem::path> Roots{Impl->Root};
	for (const auto& Source : InSources)
	{
		const auto Additional = AdditionalShaderRoot(Impl->SourcePath(Source));
		if (!Additional.empty())
		{
			Roots.insert(Additional);
		}
	}
	auto Captured = std::make_shared<FShaderSourceSnapshot::FImpl>();
	Captured->Owner = Impl->SnapshotOwner;
	for (const auto& Root : Roots)
	{
		Captured->Trees.emplace(Root, CaptureSourceTree(*Impl->Files, Root));
	}
	FShaderSourceSnapshot Result;
	Result.Impl = std::move(Captured);
	return Result;
}

namespace
{
std::string ShaderCacheKey(const std::filesystem::path& InPath, const std::filesystem::path& InRoot,
                           const std::string& InEntry, EShaderStage InStage, EShaderFormat InFormat,
                           const FShaderCompileOptions& InOptions, const FShaderSourceTrees& InSources)
{
	std::string Identity = "hyperion-shader-v10-snapshot-msl20:" HYP_TOOLCHAIN_ID;
	auto Append = [&](const std::string& InPart)
	{
		AppendIdentity(Identity, InPart);
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
	Append(InSources.at(InRoot).Digest);
	const auto Additional = AdditionalShaderRoot(InPath);
	if (!Additional.empty() && Additional != InRoot)
	{
		Append(Additional.lexically_relative(InRoot).generic_string());
		Append(InSources.at(Additional).Digest);
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
                           EShaderFormat InFormat, const FShaderCompileOptions& InOptions, IFileSystem& InFiles,
                           const FShaderSourceTrees& InSources)
{
	const auto& Content = ReadSource(InSources, InRoot, AdditionalShaderRoot(InPath), InPath);
	DxcBuffer Buffer{Content.data(), Content.size(), DXC_CP_UTF8};
	std::vector<std::wstring> Arguments{InPath.generic_wstring(),
	                                    L"-E",
	                                    std::wstring(InEntry.begin(), InEntry.end()),
	                                    L"-T",
	                                    InStage == EShaderStage::Vertex  ? L"vs_6_0"
	                                    : InStage == EShaderStage::Pixel ? L"ps_6_0"
	                                                                     : L"cs_6_0",
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
	Include.Attach(
	    new FIncludeHandler(InUtils, InRoot, InFiles, AdditionalShaderRoot(InPath), InSources, IncludeError));
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
	const std::array Sources{InSource};
	return Compile(InSource, std::move(InEntry), InStage, InFormat, std::move(InOptions), CaptureSources(Sources));
}

FShaderArtifact FShaderCompiler::Compile(const std::filesystem::path& InSource, std::string InEntry,
                                         EShaderStage InStage, EShaderFormat InFormat, FShaderCompileOptions InOptions,
                                         const FShaderSourceSnapshot& InSources)
{
	NormalizeOptions(InOptions);
	if (InEntry.empty() ||
	    (InStage != EShaderStage::Vertex && InStage != EShaderStage::Pixel && InStage != EShaderStage::Compute) ||
	    (InFormat != EShaderFormat::Dxil && InFormat != EShaderFormat::Spirv && InFormat != EShaderFormat::Msl))
	{
		throw std::invalid_argument("Unsupported shader entry, stage or target");
	}
	if (!InSources.Impl || InSources.Impl->Owner != Impl->SnapshotOwner)
	{
		throw std::invalid_argument("Shader snapshot belongs to a different compiler or is empty");
	}
	std::lock_guard Lock(Impl->Mutex);
	return Impl->Compile(Impl->SourcePath(InSource), InEntry, InStage, InFormat, InOptions, InSources.Impl->Trees);
}

FShaderArtifact FShaderCompiler::FImpl::Compile(const std::filesystem::path& InPath, const std::string& InEntry,
                                                EShaderStage InStage, EShaderFormat InFormat,
                                                const FShaderCompileOptions& InOptions,
                                                const FShaderSourceTrees& InSources)
{
	HYP_PERF_SCOPE_C(Assets, ShaderCompilation);
	// Logical HLSL types can be lowered (notably bool -> uint) by SPIR-V. Preserve their DXIL source types,
	// while reflecting every offset and stride from the actual SPIR-V intermediate.
	FShaderArtifact Logical{};
	if (InFormat != EShaderFormat::Dxil)
	{
		Logical = Compile(InPath, InEntry, InStage, EShaderFormat::Dxil, InOptions, InSources);
	}
	FShaderArtifact Artifact{};
	Artifact.Format = InFormat;
	Artifact.Stage = InStage;
	Artifact.CacheKey = ShaderCacheKey(InPath, Root, InEntry, InStage, InFormat, InOptions, InSources);
	const auto CacheFile = Cache / (Artifact.CacheKey + ".bin");
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
		Payload = CompilePayload(Compiler.Get(), Utils.Get(), InPath, Root, InEntry, InStage, InFormat, InOptions,
		                         *Files, InSources);
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
