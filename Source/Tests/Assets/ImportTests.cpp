#include "Hyperion/AssetImport/GltfImport.h"
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

int main()
{
	using namespace Hyperion;
	try
	{
		FTaskSystem Tasks(1, 1);
		auto Storage = std::make_shared<FObservedFileSystem>();
		FIOService IO(Tasks, Storage);
		FAssetService Assets(IO);
		RegisterGltfImporter(Assets);
		const auto Root = std::filesystem::path(HYP_SOURCE_DIR) / "out/fixtures";
		auto Cancelled = Assets.LoadAsync<FModelAsset>(Root / "Showcase.gltf");
		auto Shared = Assets.LoadAsync<FModelAsset>(Root / "Showcase.gltf");
		Cancelled.Cancel();
		auto Model = Shared.Get(Tasks);
		HYP_CHECK(Model->Primitives.size() == 4 && ModelInstances(*Model).size() == 4 && Model->Images[0].Width == 64);
		HYP_CHECK(Storage->Thread != std::this_thread::get_id());
		HYP_CHECK(IO.Statistics().Reads == 3);
		HYP_CHECK(Assets.LoadAsync<FModelAsset>(Root / "Showcase.gltf").Get(Tasks) == Model);
		bool Failed = false;
		try
		{
			Cancelled.GetReady();
		}
		catch (...)
		{
			Failed = true;
		}
		HYP_CHECK(Failed);
		for (const auto* File : {"Showcase.glb", "DataUri.gltf", "Jpeg.gltf"})
		{
			auto Loaded = Assets.LoadAsync<FModelAsset>(Root / File).Get(Tasks);
			HYP_CHECK(Loaded->Primitives[0].Positions == Model->Primitives[0].Positions);
			HYP_CHECK(ModelBounds(*Loaded).Maximum.X == ModelBounds(*Model).Maximum.X);
		}
		for (const auto* File : {"Sparse.gltf", "Interleaved.gltf", "SparseInterleaved.gltf"})
		{
			auto Loaded = Assets.LoadAsync<FModelAsset>(Root / File).Get(Tasks);
			HYP_CHECK(Loaded->Primitives[0].Positions == std::vector<float>({0, 0, 0, 2, 0, 0, 0, 3, 0}));
		}
		auto Normalized = Assets.LoadAsync<FModelAsset>(Root / "Normalized.gltf").Get(Tasks);
		HYP_CHECK(std::abs(Normalized->Primitives[0].Colors[5] - 128.f / 255) < .00001f);
		for (const auto* File : {"Strip.gltf", "Fan.gltf"})
		{
			auto Topology = Assets.LoadAsync<FModelAsset>(Root / File).Get(Tasks);
			HYP_CHECK(Topology->Primitives[0].Indices.size() == 6);
			for (std::size_t Index = 2; Index < Topology->Primitives[0].Normals.size(); Index += 3)
			{
				HYP_CHECK(Topology->Primitives[0].Normals[Index] == 1);
			}
		}
		for (const auto* File :
		     {"Missing.gltf", "Unsupported.gltf", "BadAccessor.gltf", "Cycle.gltf", "Lines.gltf", "Truncated.glb",
		      "InvalidSparseView.gltf", "OverflowView.gltf", "InvalidSparseCount.gltf", "DeepNodes.gltf"})
		{
			Failed = false;
			try
			{
				Assets.LoadAsync<FModelAsset>(Root / File).Get(Tasks);
			}
			catch (...)
			{
				Failed = true;
			}
			if (!Failed)
			{
				throw std::runtime_error(std::string("Expected import rejection: ") + File);
			}
		}
		Assets.SaveAsync("model-roundtrip.hasset", Model).Get(Tasks);
		auto Restored = Assets.LoadAsync<FModelAsset>("model-roundtrip.hasset").Get(Tasks);
		HYP_CHECK(Serialize(*Restored) == Serialize(*Model));
		auto Changed = std::make_shared<FModelAsset>(*Model);
		Changed->Name = "Changed snapshot";
		Assets.SaveAsync<FModelAsset>("model-roundtrip.hasset", Changed).Get(Tasks);
		HYP_CHECK(Assets.LoadAsync<FModelAsset>("model-roundtrip.hasset").Get(Tasks)->Name == "Changed snapshot");
		Failed = false;
		try
		{
			Assets.SaveAsync("unsupported.gltf", Model).Get(Tasks);
		}
		catch (...)
		{
			Failed = true;
		}
		HYP_CHECK(Failed);
		{
			FAssetService Temporary(IO);
			RegisterGltfImporter(Temporary);
			auto Abandoned = Temporary.LoadAsync<FModelAsset>(Root / "Showcase.glb");
			Abandoned.Cancel();
			// Destruction cancels/drains the producer before IO or the task system dies.
		}
		Assets.Drain();
		Tasks.Shutdown();
		std::cout << "Asynchronous glTF import and native persistence passed\n";
	}
	catch (const std::exception& InError)
	{
		std::cerr << InError.what() << '\n';
		return 1;
	}
}
