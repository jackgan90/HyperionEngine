#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Hyperion/Core/Core.h"
// DXC's Windows declarations require the COM types provided by WRL first.
#include <Windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <bcrypt.h>
#include <cstring>
#include <dxcapi.h>
#include <fstream>
#include <mutex>
#include <spirv_cross.hpp>
#include <spirv_msl.hpp>
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
	FIncludeHandler(IDxcUtils* InUtils, std::filesystem::path InRoot) : Utils(InUtils), Root(std::move(InRoot))
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
			if (Path.is_relative())
			{
				Path = Root / Path;
			}
			Path = std::filesystem::weakly_canonical(Path);
			if (!Within(Path, Root))
			{
				return E_ACCESSDENIED;
			}
			ComPtr<IDxcBlobEncoding> Blob;
			auto Hr = Utils->LoadFile(Path.c_str(), nullptr, &Blob);
			if (FAILED(Hr))
			{
				return Hr;
			}
			*OutSource = Blob.Detach();
			return S_OK;
		}
		catch (...)
		{
			return E_FAIL;
		}
	}

private:
	std::atomic<ULONG> Refs{1};
	ComPtr<IDxcUtils> Utils;
	std::filesystem::path Root;
};
} // namespace

struct FShaderCompiler::FImpl
{
	std::filesystem::path Root;
	std::filesystem::path Cache;
	std::mutex Mutex;
	ComPtr<IDxcUtils> Utils;
	ComPtr<IDxcCompiler3> Compiler;

	FImpl(std::filesystem::path InR, std::filesystem::path InC)
	    : Root(std::filesystem::canonical(InR)), Cache(std::filesystem::absolute(InC))
	{
		if (Within(Cache, Root))
		{
			throw std::invalid_argument("Shader cache must be outside source root");
		}
		std::filesystem::create_directories(Cache);
		Checked(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils)), "Create DXC utils");
		Checked(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)), "Create DXC compiler");
	}
};

FShaderCompiler::FShaderCompiler(std::filesystem::path InRoot, std::filesystem::path InCache)
    : Impl(std::make_unique<FImpl>(std::move(InRoot), std::move(InCache)))
{
}

FShaderCompiler::~FShaderCompiler() = default;

FShaderArtifact FShaderCompiler::Compile(const std::filesystem::path& InSource, std::string InEntry,
                                         EShaderStage InStage, EShaderFormat InFormat)
{
	std::lock_guard Lock(Impl->Mutex);
	FProfileScope Trace("Shader compilation");
	auto Path = std::filesystem::canonical(InSource.is_absolute() ? InSource : Impl->Root / InSource);
	if (!Within(Path, Impl->Root))
	{
		throw std::invalid_argument("Shader outside source root");
	}
	std::string Identity = "hyperion-shader-v2:" HYP_TOOLCHAIN_ID;
	auto Append = [&](const std::string& InPart)
	{
		Identity += std::to_string(InPart.size()) + ":" + InPart;
	};
	Append(Path.lexically_relative(Impl->Root).generic_string());
	Append(InEntry);
	Append(std::to_string(static_cast<int>(InStage)));
	Append(std::to_string(static_cast<int>(InFormat)));
	std::vector<std::filesystem::path> Files;
	for (const auto& Item : std::filesystem::recursive_directory_iterator(Impl->Root))
	{
		if (Item.is_regular_file())
		{
			Files.push_back(Item.path());
		}
	}
	std::sort(Files.begin(), Files.end());
	for (const auto& File : Files)
	{
		Append(File.lexically_relative(Impl->Root).generic_string());
		Append(Read(File));
	}
	FShaderArtifact Artifact{};
	Artifact.Format = InFormat;
	Artifact.CacheKey = Sha256(Identity);
	auto CacheFile = Impl->Cache / (Artifact.CacheKey + ".bin");
	std::string Payload;
	if (std::filesystem::exists(CacheFile))
	{
		auto Cached = Read(CacheFile);
		if (Cached.size() > 65 && Cached[64] == '\n' && Sha256(Cached.substr(65)) == Cached.substr(0, 64))
		{
			Payload = Cached.substr(65);
			Artifact.CacheHit = true;
		}
	}
	if (!Artifact.CacheHit)
	{
		auto Content = Read(Path);
		DxcBuffer Buffer{Content.data(), Content.size(), DXC_CP_UTF8};
		std::wstring WideEntry(InEntry.begin(), InEntry.end());
		auto Parent = Path.parent_path().wstring();
		auto Root = Impl->Root.wstring();
		auto Filename = Path.wstring();
		const wchar_t* Profile = InStage == EShaderStage::Vertex ? L"vs_6_0" : L"ps_6_0";
		std::vector<LPCWSTR> Args{Filename.c_str(), L"-E",   WideEntry.c_str(), L"-T",  Profile,
		                          L"-HV",           L"2021", L"-Ges",           L"-O3", L"-I",
		                          Parent.c_str(),   L"-I",   Root.c_str()};
		if (InFormat != EShaderFormat::Dxil)
		{
			const LPCWSTR SpirvArgs[] = {L"-spirv",
			                             L"-fspv-target-env=vulkan1.1",
			                             L"-fvk-t-shift",
			                             L"1000",
			                             L"0",
			                             L"-fvk-s-shift",
			                             L"2000",
			                             L"0",
			                             L"-fvk-u-shift",
			                             L"3000",
			                             L"0"};
			Args.insert(Args.end(), std::begin(SpirvArgs), std::end(SpirvArgs));
		}
		ComPtr<IDxcIncludeHandler> Include;
		Include.Attach(new FIncludeHandler(Impl->Utils.Get(), Impl->Root));
		ComPtr<IDxcResult> Result;
		Checked(Impl->Compiler->Compile(&Buffer, Args.data(), static_cast<UINT32>(Args.size()), Include.Get(),
		                                IID_PPV_ARGS(&Result)),
		        "DXC invocation failed");
		HRESULT Status{};
		Checked(Result->GetStatus(&Status), "DXC status failed");
		if (FAILED(Status))
		{
			ComPtr<IDxcBlobUtf8> Errors;
			Result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&Errors), nullptr);
			throw std::runtime_error(Errors ? Errors->GetStringPointer() : "Shader compilation failed");
		}
		ComPtr<IDxcBlob> Object;
		Checked(Result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&Object), nullptr), "DXC object missing");
		Payload.assign(static_cast<const char*>(Object->GetBufferPointer()), Object->GetBufferSize());
		// One immutable filename per content key; a partial/concurrent write is rejected by the digest on
		// next read.
		std::ofstream Out(CacheFile, std::ios::binary | std::ios::trunc);
		Out << Sha256(Payload) << '\n';
		Out.write(Payload.data(), static_cast<std::streamsize>(Payload.size()));
		if (!Out)
		{
			throw std::runtime_error("Shader cache write failed");
		}
	}
	if (InFormat != EShaderFormat::Dxil)
	{
		if (Payload.size() % 4)
		{
			throw std::runtime_error("Invalid SPIR-V size");
		}
		std::vector<std::uint32_t> Words(Payload.size() / 4);
		std::memcpy(Words.data(), Payload.data(), Payload.size());
		spirv_cross::CompilerMSL Cross(std::move(Words));
		auto Resources = Cross.get_shader_resources();
		auto Reflect = [&](const auto& InList, EBindingKind InKind)
		{
			for (const auto& R : InList)
			{
				std::uint32_t Size{};
				if (InKind == EBindingKind::UniformBuffer)
				{
					Size = static_cast<std::uint32_t>(Cross.get_declared_struct_size(Cross.get_type(R.base_type_id)));
				}
				Artifact.Bindings.push_back({R.name, InKind, Cross.get_decoration(R.id, spv::DecorationBinding),
				                             Cross.get_decoration(R.id, spv::DecorationDescriptorSet), Size});
			}
		};
		Reflect(Resources.uniform_buffers, EBindingKind::UniformBuffer);
		Reflect(Resources.separate_images, EBindingKind::Texture);
		Reflect(Resources.separate_samplers, EBindingKind::Sampler);
		if (InFormat == EShaderFormat::Msl)
		{
			Payload = Cross.compile();
		}
	}
	Artifact.Bytes.assign(Payload.begin(), Payload.end());
	return Artifact;
}
} // namespace Hyperion
