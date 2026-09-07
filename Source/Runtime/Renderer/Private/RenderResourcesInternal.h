#pragma once
#include "Hyperion/Renderer/RenderResources.h"
#include <atomic>
#include <map>
#include <mutex>
#include <tuple>

namespace Hyperion
{
struct FRenderResourceRecord
{
	FTaskSystem& Tasks;
	std::uint64_t Identity{};
	const void* Owner{};
	std::shared_ptr<const void> Asset;
	TAsyncResult<FRenderResourceDesc> Preparation;
	mutable std::mutex Publication;
	ERenderResourceStatus Status = ERenderResourceStatus::Preparing;
	std::string Error;
	bool bUploadPending{};
	std::shared_ptr<const FRenderResourceDesc> Description;
	// Coordinator only. Every record stays pinned until leases and native draw references finish.
	std::vector<FBuffer> Vertices;
	std::vector<FBuffer> Indices;
	std::vector<FTexture> Textures;
	std::vector<std::array<FPipeline, 2>> Pipelines;

	explicit FRenderResourceRecord(FTaskSystem& InTasks) : Tasks(InTasks)
	{
	}

	~FRenderResourceRecord();
	void Publish(ERenderResourceStatus InStatus, std::string InError = {});
	bool CanRelease() const;
	void Release();
};

struct FRenderResourceCoordinator : std::enable_shared_from_this<FRenderResourceCoordinator>
{
	using FKey = std::tuple<const void*, std::uint64_t, std::string, std::uint64_t>;

	struct FEntry
	{
		std::shared_ptr<FRenderResourceRecord> Record;
		std::weak_ptr<const FRenderResource> Lease;
	};

	FTaskSystem& Tasks;
	IRHIDevice& Device;
	mutable std::mutex Mutex;
	bool bClosed{};
	bool bScheduled{};
	FTaskHandle Progress;
	std::map<FKey, FEntry> Entries;
	std::vector<FBuffer> Constants;
	FRenderResourceStats Stats;

	FRenderResourceCoordinator(FTaskSystem& InTasks, IRHIDevice& InDevice) : Tasks(InTasks), Device(InDevice)
	{
	}

	std::shared_ptr<const FRenderResource> MakeLease(FEntry& InEntry);
	void Schedule();
	void Tick();
	bool CollectCompleted();
	void Upload(FRenderResourceRecord& InRecord);
	bool Process(FEntry& InEntry);
	FDrawPacket Draw(const FRenderItem& InItem, const FRenderView& InView);
};
} // namespace Hyperion
