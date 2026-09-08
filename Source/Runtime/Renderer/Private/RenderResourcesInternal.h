#pragma once
#include "Hyperion/Renderer/RenderResources.h"
#include <atomic>
#include <map>
#include <mutex>
#include <tuple>

namespace Hyperion
{
struct FMaterialProgramRecord
{
	std::shared_ptr<const FMaterialDefinition> Definition;
	TAsyncResult<FCompiledMaterialDefinition> Preparation;
	std::shared_ptr<const FCompiledMaterialDefinition> Compiled;
	std::string Error;
};

struct FRenderMaterialRecord
{
	FTaskSystem& Tasks;
	const void* Owner{};
	std::shared_ptr<FMaterialProgramRecord> Program;
	std::shared_ptr<const FMaterialSnapshot> StaticSnapshot;
	FMaterialParameterValues ResourceSignature;
	mutable std::mutex Publication;
	ERenderMaterialStatus Status = ERenderMaterialStatus::Preparing;
	std::string Error;
	std::shared_ptr<const FCompiledMaterialDefinition> Compiled;
	std::vector<FMaterialResourceBindings> StaticBindings;
	std::shared_ptr<const void> GpuLifetime = std::make_shared<const int>(0);

	explicit FRenderMaterialRecord(FTaskSystem& InTasks) : Tasks(InTasks)
	{
	}

	~FRenderMaterialRecord();
	void Publish(ERenderMaterialStatus InStatus, std::string InError = {});
	void Release();
};

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
	std::shared_ptr<const FRenderResourceDesc> Description;
	// Coordinator only. Every record stays pinned until leases and native draw references finish.
	std::vector<FBuffer> Vertices;
	std::vector<FBuffer> Indices;
	std::vector<std::shared_ptr<const FRenderMaterial>> Materials;

	explicit FRenderResourceRecord(FTaskSystem& InTasks) : Tasks(InTasks)
	{
	}

	~FRenderResourceRecord();
	void Publish(ERenderResourceStatus InStatus, std::string InError = {});
	bool CanRelease() const;
	std::vector<std::shared_ptr<const FRenderMaterial>> Release();
};

struct FRenderResourceCoordinator : std::enable_shared_from_this<FRenderResourceCoordinator>
{
	using FKey = std::tuple<const void*, std::uint64_t, std::string, std::uint64_t>;
	// A null prepared selection denotes the device's automatic definition compilation.
	using FProgramKey = std::pair<std::uint64_t, std::shared_ptr<const FCompiledMaterialDefinition>>;

	struct FEntry
	{
		std::shared_ptr<FRenderResourceRecord> Record;
		std::weak_ptr<const FRenderResource> Lease;
	};

	struct FMaterialEntry
	{
		std::shared_ptr<FRenderMaterialRecord> Record;
		std::vector<std::weak_ptr<const FRenderMaterial>> Leases;
	};

	struct FPreparedDraw
	{
		std::weak_ptr<const FResolvedMaterialParameters> Parameters;
		std::weak_ptr<const void> Resources;
		std::vector<std::shared_ptr<const FMaterialValue>> ResourceValues;
		std::weak_ptr<const FRenderResource> Geometry;
		std::weak_ptr<const FRenderMaterial> Surface;
		FGraphicsTarget Target;
		bool bMirrored{};
		FDrawPacket Packet;
	};

	using FDrawKey = std::tuple<const void*, const FRenderResource*, std::uint32_t, std::string>;
	std::map<FDrawKey, FPreparedDraw> PreparedDraws;
	void CollectPreparedDraws();
	FTaskSystem& Tasks;
	IRHIDevice& Device;
	mutable std::mutex Mutex;
	bool bClosed{};
	bool bNativeClosed{};
	bool bScheduled{};
	FTaskHandle Progress;
	std::map<FKey, FEntry> Entries;
	std::map<FProgramKey, std::shared_ptr<FMaterialProgramRecord>> Programs;
	std::vector<FMaterialEntry> MaterialEntries;
	std::unique_ptr<FMaterialGpuCache> MaterialGpu;
	std::unique_ptr<FMaterialConstantCache> MaterialConstants;
	FShaderCompiler& Compiler;
	FRenderResourceStats Stats;

	FRenderResourceCoordinator(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler)
	    : Tasks(InTasks), Device(InDevice), Compiler(InCompiler)
	{
	}

	std::shared_ptr<const FRenderResource> MakeLease(FEntry& InEntry);
	std::shared_ptr<const FRenderMaterial> AcquireMaterial(
	    std::shared_ptr<const FMaterialSnapshot> InSnapshot,
	    std::shared_ptr<const FCompiledMaterialDefinition> InCompiled = {});
	std::shared_ptr<const FRenderMaterial> MakeMaterialLease(FMaterialEntry& InEntry,
	                                                         std::shared_ptr<const FMaterialSnapshot> InSnapshot);
	void EnsureMaterialCaches();
	bool ProcessMaterials();
	void PrepareMaterialResources(FRenderMaterialRecord& InRecord);
	void CloseNativeResources();
	void Schedule();
	void Tick();
	bool CollectCompleted();
	void Upload(FRenderResourceRecord& InRecord);
	bool Process(FEntry& InEntry);
	FDrawPacket DrawMaterial(const FRenderItem& InItem, const FRenderView& InView, FGraphicsTarget InTarget);
};
} // namespace Hyperion
