#pragma once
#include "DepthPreview.h"
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
	FRenderResourceCoordinator* Owner{};
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
	FRenderResourceCoordinator* Owner{};
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
		FMaterialConstantState Constants;
		std::weak_ptr<const FMaterialSharedParameters> Shared;
	};

	using FDrawKey = std::tuple<const void*, const FRenderResource*, std::uint32_t, std::string, bool>;
	std::map<FDrawKey, FPreparedDraw> PreparedDraws;

	struct FPreparedViewDraw
	{
		std::size_t Item{};
		std::optional<std::size_t> Batch;
	};

	struct FPreparedViewPasses
	{
		std::weak_ptr<const void> Contents;
		std::uint64_t ResourceRevision{};
		std::vector<FGraphicsDrawBatch> Passes;
		FRenderBatchStats Statistics;
		std::weak_ptr<const void> LocalContents;
		std::weak_ptr<const void> BatchStructure;
		std::vector<FPreparedViewDraw> Sources;
	};

	using FViewKey = std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>;
	std::map<FViewKey, FPreparedViewPasses> PreparedViews;

	struct FBatchAdmission
	{
		std::weak_ptr<const void> Contents;
		std::uint64_t ResourceRevision{};
		std::string Usage;
		bool bSrgb{};
	};

	std::map<const void*, FBatchAdmission> BatchAdmissions;
	bool ReuseViewPasses(const FRenderSceneSnapshot& InSnapshot, std::vector<FGraphicsDrawBatch>& OutPasses);
	void CacheViewPasses(const FRenderSceneSnapshot& InSnapshot, std::vector<FGraphicsDrawBatch>& InPasses,
	                     std::vector<FPreparedViewDraw> InSources);
	bool RefreshViewPasses(const FRenderSceneSnapshot& InSnapshot, FPreparedViewPasses& InCached,
	                       std::vector<FGraphicsDrawBatch>& OutPasses);
	void CollectViewPasses();
	void CollectPreparedDraws();
	FTaskSystem& Tasks;
	IRHIDevice& Device;
	mutable std::mutex Mutex;
	bool bClosed{};
	bool bNativeClosed{};
	bool bScheduled{};
	std::atomic_uint64_t PublicationRevision{1};

	struct FScopeLifetime
	{
		std::atomic<bool> bUsed{};
	};

	std::mutex ScopeMutex;
	std::map<const void*, std::weak_ptr<FScopeLifetime>> ScopeLifetimes;
	void TrackScope(const std::shared_ptr<const void>& InScope);
	void ReleaseScope(FScopeLifetime* InScope);
	FTaskHandle Progress;
	std::map<FKey, FEntry> Entries;
	std::map<FProgramKey, std::shared_ptr<FMaterialProgramRecord>> Programs;
	std::vector<FMaterialEntry> MaterialEntries;
	std::unique_ptr<FMaterialGpuCache> MaterialGpu;
	std::unique_ptr<FMaterialConstantCache> MaterialConstants;
	FShaderCompiler& Compiler;
	FRenderResourceStats Stats;
	FDepthPreview DepthPreview;

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
	FDrawPacket DrawMaterial(const FRenderItem& InItem, const FRenderView& InView, FGraphicsTarget InTarget,
	                         bool bInInstance = false);
	void ValidateDrawItem(const FRenderItem& InItem, const FRenderView& InView);
	void BindInstances(FDrawPacket& InPacket, std::shared_ptr<const FInstanceBatchData> InData);
};
} // namespace Hyperion
