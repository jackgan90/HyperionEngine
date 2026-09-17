#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/IO/IOService.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "Support/TestSupport.h"
#include <condition_variable>
#include <future>
#include <iostream>

using namespace Hyperion;

namespace
{
class FGatedFiles final : public IFileSystem
{
public:
	FMemoryFileSystem Storage;
	std::mutex Mutex;
	std::condition_variable Changed;
	unsigned Entered{};
	bool bEnabled{};
	bool bReleased{};
	bool bTimedOut{};

	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override
	{
		return Storage.Read(InPath, InLimit);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		Storage.WriteAtomic(InPath, InBytes);
	}

	std::vector<FFileContents> ReadTree(const std::filesystem::path& InPath,
	                                    std::span<const std::string_view> InExtensions, std::size_t InLimit) override
	{
		{
			std::unique_lock Lock(Mutex);
			if (bEnabled)
			{
				++Entered;
				Changed.notify_all();
				if (!Changed.wait_for(Lock, std::chrono::seconds(10),
				                      [&]
				                      {
					                      return bReleased;
				                      }))
				{
					bTimedOut = true;
					bReleased = true;
					bEnabled = false;
					Changed.notify_all();
				}
			}
		}
		return Storage.ReadTree(InPath, InExtensions, InLimit);
	}

	void Arm()
	{
		std::lock_guard Lock(Mutex);
		bEnabled = true;
	}

	void WaitForCompile()
	{
		std::unique_lock Lock(Mutex);
		HYP_CHECK(Changed.wait_for(Lock, std::chrono::seconds(5),
		                           [&]
		                           {
			                           return Entered == 1;
		                           }));
	}

	void Release()
	{
		std::lock_guard Lock(Mutex);
		bReleased = true;
		bEnabled = false;
		Changed.notify_all();
	}
};

void Write(FGatedFiles& InFiles, const std::string& InName, std::string_view InText)
{
	InFiles.WriteAtomic("/Engine/Shaders/" + InName, std::as_bytes(std::span(InText.data(), InText.size())));
}

std::shared_ptr<const FMaterialAssetData> Material(const std::string& InName, std::string InPath = "Main.hlsl")
{
	auto Asset = std::make_shared<FMaterialAsset>();
	Asset->Name = InName;
	FMaterialPass Pass;
	Pass.Vertex = {InPath, "VSMain"};
	Pass.Pixel = {InPath, "PSMain"};
	Asset->Passes.push_back(std::move(Pass));
	return std::make_shared<const FMaterialAssetData>(FMaterialAssetData{std::move(Asset)});
}

constexpr std::string_view Shader = "float4 VSMain(float3 p:POSITION):SV_Position{return float4(p,1);}"
                                    "float4 PSMain():SV_Target{return float4(1,0,0,1);}";

void CheckIndependentRequests()
{
	auto Files = std::make_shared<FGatedFiles>();
	Write(*Files, "Main.hlsl", Shader);
	Write(*Files, "Second.hlsl", Shader);
	FShaderCompiler First("/Engine/Shaders", "material-cache-test/first", Files);
	FTaskSystem Tasks(3, 1);
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	std::unique_ptr<IRHIDevice> Device;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
	                          }));
	FRenderResourceService Cache(Tasks, *Device, First);
	const auto ReadyData = Material("Ready");
	std::vector<std::shared_ptr<const FMaterialSnapshot>> Retained;
	Retained.push_back(Cache.PrepareMaterialAsset(ReadyData));
	for (unsigned Index = 0; Index < 1022; ++Index)
	{
		Retained.push_back(Cache.PrepareMaterialAsset(Material(std::to_string(Index))));
	}
	HYP_CHECK(Cache.Statistics().AssetMaterials.MaterialEntries == 1023);
	const auto SlowData = Material("First compilation");
	const auto OtherData = Material("Second compilation", "Second.hlsl");
	Files->Arm();
	auto Slow = std::async(std::launch::async,
	                       [&]
	                       {
		                       return Cache.PrepareMaterialAsset(SlowData);
	                       });
	Files->WaitForCompile();
	auto Other = std::async(std::launch::async,
	                        [&]
	                        {
		                        return Cache.PrepareMaterialAsset(OtherData);
	                        });
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (Cache.Statistics().AssetMaterials.Requests < 1025)
	{
		HYP_CHECK(std::chrono::steady_clock::now() < Deadline);
		std::this_thread::yield();
	}
	auto Same = std::async(std::launch::async,
	                       [&]
	                       {
		                       return Cache.PrepareMaterialAsset(SlowData);
	                       });
	while (Cache.Statistics().AssetMaterials.Requests < 1026)
	{
		HYP_CHECK(std::chrono::steady_clock::now() < Deadline);
		std::this_thread::yield();
	}
	// Both keys are admitted, while the first compile is stopped in IO. Reads must complete before release.
	auto ReadyLease = Cache.RequestMaterial(Retained.front());
	HYP_CHECK(ReadyLease);
	HYP_CHECK(Cache.Statistics().AssetMaterials.MaterialEntries == 1023);
	const auto Hit = Cache.PrepareMaterialAsset(ReadyData);
	HYP_CHECK(Hit == Retained.front());
	Files->Release();
	const auto A = Slow.get();
	const auto B = Other.get();
	const auto Shared = Same.get();
	HYP_CHECK(!Files->bTimedOut);
	HYP_CHECK(A == Shared);
	HYP_CHECK(B && Cache.Statistics().AssetMaterials.MaterialEntries <= 1024);
	HYP_CHECK(Cache.Statistics().AssetMaterials.MaterialPreparations == 1025);

	Write(*Files, "Broken.hlsl", "invalid shader");
	const auto Broken = Material("Retry", "Broken.hlsl");
	bool bRejected{};
	try
	{
		(void)Cache.PrepareMaterialAsset(Broken);
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	Write(*Files, "Broken.hlsl", Shader);
	HYP_CHECK(Cache.PrepareMaterialAsset(Broken));
	ReadyLease.reset();
	Retained.clear();
	Cache.Close();
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Device->WaitIdle();
		                          HYP_CHECK(Device->Statistics().ValidationErrors == 0);
		                          Device.reset();
	                          }));
}
} // namespace

int main()
{
	try
	{
		CheckIndependentRequests();
		std::cout << "Material cache independent reads, same-key sharing, concurrent bound and retry passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
