#pragma once
#include "Hyperion/Renderer/MaterialConstantCache.h"
#include "Hyperion/Renderer/MaterialGpuCache.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Renderer/RenderMaterial.h"
#include "Hyperion/Renderer/RenderPrimitive.h"
#include "Hyperion/Tasks/AsyncResult.h"

namespace Hyperion
{
struct FRenderGeometryDesc
{
	std::vector<std::byte> Vertices;
	std::vector<std::uint32_t> Indices;
	std::uint32_t VertexStride{};
	FBounds Bounds;
	std::vector<FVertexAttribute> Attributes;
	ERHIPrimitiveTopology Topology = ERHIPrimitiveTopology::TriangleList;
};

struct FRenderMaterialDesc
{
	std::shared_ptr<const FMaterialSnapshot> Surface;
	std::shared_ptr<const FCompiledMaterialDefinition> Compiled;
};

struct FRenderSection
{
	std::uint32_t Geometry{};
	std::uint32_t Material{};
	std::uint32_t FirstIndex{};
	std::uint32_t IndexCount{};
};

struct FRenderResourceDesc
{
	std::vector<FRenderGeometryDesc> Geometries;
	std::vector<FRenderMaterialDesc> Materials;
	std::vector<FRenderSection> Sections;
};

enum class ERenderResourceStatus
{
	Preparing,
	Uploading,
	Ready,
	Failed,
	Retired
};

struct FRenderResourceRecord;
struct FRenderResourceCoordinator;

// Thread-neutral lease. Native resources are private and pinned by the RHI coordinator.
class FRenderResource
{
public:
	~FRenderResource();
	ERenderResourceStatus GetStatus() const;
	std::string GetError() const;
	std::uint64_t GetIdentity() const;
	std::shared_ptr<const FRenderResourceDesc> GetDescription() const;
	std::shared_ptr<const FRenderMaterial> GetMaterial(std::uint32_t InSection) const;

private:
	FRenderResource(std::shared_ptr<FRenderResourceRecord> InRecord, std::function<void()> InReleased);
	std::shared_ptr<FRenderResourceRecord> Record;
	std::function<void()> Released;
	friend struct FRenderResourceCoordinator;
	friend class FRenderResourceService;
};

struct FRenderResourceStats
{
	std::uint64_t Requests{};
	std::uint64_t Productions{};
	std::uint64_t GeometryUploads{};
	std::uint64_t Retired{};
	std::uint64_t MaintenanceTasks{};
	std::uint64_t MaintenanceTicks{};
	std::size_t LiveResources{};
	FMaterialConstantStats Constants;
	FMaterialGpuStats Materials;
	FRenderBatchStats Batches;
};

// Owned RHI preparation endpoint. Deferred work can outlive the service facade and rejects a closed coordinator.
struct FFullscreenPassDesc;

class FRenderResourcePreparation
{
public:
	FGraphicsPass DeclarePass(FRenderGraph& InGraph, const FRenderSceneSnapshot& InSnapshot) const;
	FGraphicsDrawBatch BuildFullscreen(const FFullscreenPassDesc& InPass) const;
	FTexture ResolveTexture(std::shared_ptr<const FMaterialTextureSource> InSource,
	                        std::shared_ptr<const void> InLifetime) const;
	std::vector<FGraphicsDrawBatch> BuildDraws(const FRenderSceneSnapshot& InSnapshot,
	                                           FRenderBatchStats* OutStatistics = nullptr) const;
	FGraphicsDrawBatch BuildDepthPreview(std::shared_ptr<const FMaterialTextureSource> InSource,
	                                     std::shared_ptr<const void> InLifetime, FViewport InViewport) const;

private:
	explicit FRenderResourcePreparation(std::shared_ptr<FRenderResourceCoordinator> InCoordinator)
	    : Coordinator(std::move(InCoordinator))
	{
	}

	std::shared_ptr<FRenderResourceCoordinator> Coordinator;
	friend class FRenderResourceService;
	friend class FRenderSession;
	FGraphicsPass DeclarePass(FRenderGraph& InGraph, const FRenderSceneSnapshot& InSnapshot,
	                          std::span<const FRenderTargetSource> InReads) const;
};

// One service per rendering device/session, shared by all its scene producers.
// Request/Close: Main. BuildDraws: RHI 0. Queries: any domain.
class FRenderResourceService
{
public:
	FRenderResourceService(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler);
	~FRenderResourceService();
	FRenderResourceService(const FRenderResourceService&) = delete;
	FRenderResourceService& operator=(const FRenderResourceService&) = delete;
	std::shared_ptr<const FRenderResource> RequestModel(std::shared_ptr<const FModelAsset> InAsset,
	                                                    std::uint64_t InVersion = 1);
	std::shared_ptr<const FRenderMaterial> RequestMaterial(std::shared_ptr<const FMaterialSnapshot> InSnapshot);
	std::shared_ptr<const void> CreateScopeLifetime() const;
	// Changes only when resource readiness/error publication changes; safe to observe from Main or Render.
	std::uint64_t GetPublicationRevision() const;
	std::shared_ptr<const FRenderResource> Request(std::shared_ptr<const void> InIdentity, std::uint64_t InVersion,
	                                               std::string InConfiguration,
	                                               std::function<FRenderResourceDesc()> InPrepare);
	std::vector<FGraphicsDrawBatch> BuildDraws(const FRenderSceneSnapshot& InSnapshot);
	FRenderResourcePreparation GetPreparation() const;
	FGraphicsDrawBatch BuildDepthPreview(std::shared_ptr<const FMaterialTextureSource> InSource,
	                                     std::shared_ptr<const void> InLifetime, FViewport InViewport);
	FRenderResourceStats Statistics() const;
	void Close();

private:
	std::shared_ptr<FRenderResourceCoordinator> Coordinator;
	FShaderCompiler& Compiler;
};

// Render only: freeze resource readiness, conservatively cull and globally order items.
FRenderSceneSnapshot PrepareSceneSnapshot(FRenderSceneSnapshot InSnapshot);
FRenderResourceDesc PrepareModelResources(std::shared_ptr<const FModelAsset> InAsset, FShaderCompiler& InCompiler,
                                          EShaderFormat InFormat);
} // namespace Hyperion
