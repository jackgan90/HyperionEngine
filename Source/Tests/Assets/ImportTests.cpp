#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/ModelImport.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <thread>

namespace Hyperion
{
class FObservedFileSystem final : public IFileSystem
{
public:
	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override
	{
		if (Thread == std::thread::id{})
		{
			Thread = std::this_thread::get_id();
		}
		HYP_CHECK(Thread == std::this_thread::get_id());
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return Local.Read(InPath, InLimit);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		Local.WriteAtomic(InPath, InBytes);
	}

	std::thread::id Thread;

private:
	FLocalFileSystem Local;
};
} // namespace Hyperion

namespace
{
using namespace Hyperion;

void CheckInvalidImports(FAssetImportService& InAssets, FTaskSystem& InTasks, const std::filesystem::path& InRoot)
{
	bool bFailed = false;
	for (const auto* File :
	     {"Missing.gltf", "Unsupported.gltf", "BadAccessor.gltf", "Cycle.gltf", "Lines.gltf", "Truncated.glb",
	      "InvalidSparseView.gltf", "OverflowView.gltf", "InvalidSparseCount.gltf", "DeepNodes.gltf"})
	{
		bFailed = false;
		try
		{
			InAssets.LoadAsync<FModelSource>(InRoot / File).Get(InTasks);
		}
		catch (...)
		{
			bFailed = true;
		}
		if (!bFailed)
		{
			throw std::runtime_error(std::string("Expected import rejection: ") + File);
		}
	}
}

} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		FTaskSystem Tasks(1, 1);
		auto Storage = std::make_shared<FObservedFileSystem>();
		FIOService IO(Tasks, Storage);
		FAssetImportService Assets(IO);
		FAssetService Native(IO);
		RegisterGltfImporter(Assets);
		const auto Root = std::filesystem::path(HYP_SOURCE_DIR) / "out/fixtures";
		auto Cancelled = Assets.LoadAsync<FModelSource>(Root / "Showcase.gltf");
		auto Shared = Assets.LoadAsync<FModelSource>(Root / "Showcase.gltf");
		Cancelled.Cancel();
		auto Model = Shared.Get(Tasks);
		HYP_CHECK(Model->Primitives.size() == 4 && ModelInstances(SplitModelSource(*Model).Model).size() == 4 &&
		          Model->Images[0].Width == 64);
		HYP_CHECK(Storage->Thread != std::this_thread::get_id());
		HYP_CHECK(IO.Statistics().Reads == 3);
		HYP_CHECK(Assets.LoadAsync<FModelSource>(Root / "Showcase.gltf").Get(Tasks) == Model);
		bool bFailed = false;
		try
		{
			Cancelled.GetReady();
		}
		catch (...)
		{
			bFailed = true;
		}
		HYP_CHECK(bFailed);
		for (const auto* File : {"Showcase.glb", "DataUri.gltf", "Jpeg.gltf"})
		{
			auto Loaded = Assets.LoadAsync<FModelSource>(Root / File).Get(Tasks);
			HYP_CHECK(Loaded->Primitives[0].Positions == Model->Primitives[0].Positions);
			HYP_CHECK(ModelBounds(SplitModelSource(*Loaded).Model).Maximum.X ==
			          ModelBounds(SplitModelSource(*Model).Model).Maximum.X);
		}
		for (const auto* File : {"Sparse.gltf", "Interleaved.gltf", "SparseInterleaved.gltf"})
		{
			auto Loaded = Assets.LoadAsync<FModelSource>(Root / File).Get(Tasks);
			HYP_CHECK(Loaded->Primitives[0].Positions == std::vector<float>({0, 0, 0, 2, 0, 0, 0, 3, 0}));
		}
		auto Normalized = Assets.LoadAsync<FModelSource>(Root / "Normalized.gltf").Get(Tasks);
		HYP_CHECK(std::abs(Normalized->Primitives[0].Colors[5] - 128.f / 255) < .00001f);
		for (const auto* File : {"Strip.gltf", "Fan.gltf"})
		{
			auto Topology = Assets.LoadAsync<FModelSource>(Root / File).Get(Tasks);
			HYP_CHECK(Topology->Primitives[0].Indices.size() == 6);
			for (std::size_t Index = 2; Index < Topology->Primitives[0].Normals.size(); Index += 3)
			{
				HYP_CHECK(Topology->Primitives[0].Normals[Index] == 1);
			}
		}
		CheckInvalidImports(Assets, Tasks, Root);
		const auto NativeModel = std::make_shared<const FModelAsset>(SplitModelSource(*Model).Model);
		Native.SaveAsync("model-roundtrip.hasset", NativeModel).Get(Tasks);
		auto Restored = Native.LoadAsync<FModelAsset>("model-roundtrip.hasset").Get(Tasks);
		HYP_CHECK(Serialize(*Restored) == Serialize(*NativeModel));
		auto Changed = std::make_shared<FModelAsset>(*NativeModel);
		Changed->Name = "Changed snapshot";
		Native.SaveAsync<FModelAsset>("model-roundtrip.hasset", Changed).Get(Tasks);
		HYP_CHECK(Native.LoadAsync<FModelAsset>("model-roundtrip.hasset").Get(Tasks)->Name == "Changed snapshot");
		bFailed = false;
		try
		{
			Native.SaveAsync("unsupported.gltf", NativeModel).Get(Tasks);
		}
		catch (...)
		{
			bFailed = true;
		}
		HYP_CHECK(bFailed);
		{
			FAssetImportService Temporary(IO);
			RegisterGltfImporter(Temporary);
			auto Abandoned = Temporary.LoadAsync<FModelSource>(Root / "Showcase.glb");
			Abandoned.Cancel();
			// Destruction cancels/drains the producer before IO or the task system dies.
		}
		Assets.Drain();
		Native.Drain();
		Tasks.Shutdown();
		std::cout << "Asynchronous glTF import and native persistence passed\n";
	}
	catch (const std::exception& InError)
	{
		std::cerr << InError.what() << '\n';
		return 1;
	}
}
