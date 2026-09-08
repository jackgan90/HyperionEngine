#include "RenderResourcesInternal.h"
#include <atomic>
#include <stdexcept>

namespace Hyperion
{
FRenderResourceService::FRenderResourceService(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler)
    : Coordinator(std::make_shared<FRenderResourceCoordinator>(InTasks, InDevice, InCompiler)), Compiler(InCompiler)
{
	InTasks.Require({EDomain::Main});
}

FRenderResourceService::~FRenderResourceService()
{
	Close();
}

std::shared_ptr<const FRenderResource> FRenderResourceService::RequestModel(std::shared_ptr<const FModelAsset> InAsset,
                                                                            std::uint64_t InVersion)
{
	const auto Format = Coordinator->Device.GetCapabilities().ShaderFormat;
	return Request(InAsset, InVersion, "ModelVertex-v1",
	               [Asset = InAsset, Compiler = &Compiler, Format]
	               {
		               return PrepareModelResources(Asset, *Compiler, Format);
	               });
}

std::shared_ptr<const FRenderResource> FRenderResourceService::Request(std::shared_ptr<const void> InIdentity,
                                                                       std::uint64_t InVersion,
                                                                       std::string InConfiguration,
                                                                       std::function<FRenderResourceDesc()> InPrepare)
{
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Main});
	if (!InIdentity || !InPrepare || !InVersion)
	{
		throw std::invalid_argument("Invalid render resource request");
	}
	std::shared_ptr<const FRenderResource> Lease;
	{
		std::lock_guard Lock(Owner.Mutex);
		if (Owner.bClosed)
		{
			throw std::logic_error("Render resource service is closed");
		}
		++Owner.Stats.Requests;
		FRenderResourceCoordinator::FKey RetryKey{InIdentity.get(), InVersion, std::move(InConfiguration), 0};
		for (auto It = Owner.Entries.lower_bound(RetryKey); It != Owner.Entries.end(); ++It)
		{
			const auto& Key = It->first;
			if (std::get<0>(Key) != std::get<0>(RetryKey) || std::get<1>(Key) != std::get<1>(RetryKey) ||
			    std::get<2>(Key) != std::get<2>(RetryKey))
			{
				break;
			}
			if (It->second.Record->Status != ERenderResourceStatus::Failed)
			{
				return Owner.MakeLease(It->second);
			}
			std::get<3>(RetryKey) = std::get<3>(Key) + 1;
		}
		static std::atomic_uint64_t NextIdentity{1};
		auto Record = std::make_shared<FRenderResourceRecord>(Owner.Tasks);
		Record->Identity = NextIdentity.fetch_add(1);
		Record->Owner = &Owner;
		Record->Asset = std::move(InIdentity);
		Record->Preparation = DispatchAsync<FRenderResourceDesc>(Owner.Tasks, {EDomain::Worker}, std::move(InPrepare));
		auto& Entry =
		    Owner.Entries.emplace(std::move(RetryKey), FRenderResourceCoordinator::FEntry{Record, {}}).first->second;
		Lease = Owner.MakeLease(Entry);
		++Owner.Stats.Productions;
		Owner.Stats.LiveResources = Owner.Entries.size();
	}
	Owner.Schedule();
	return Lease;
}

void FRenderResourceCoordinator::Upload(FRenderResourceRecord& InRecord)
{
	Tasks.Require({EDomain::Rhi, 0});
	const auto& Desc = *InRecord.Description;
	for (const auto& Geometry : Desc.Geometries)
	{
		if (!Geometry.VertexStride || Geometry.Vertices.empty() || Geometry.Indices.empty() ||
		    Geometry.Vertices.size() % Geometry.VertexStride)
		{
			throw std::invalid_argument("Invalid render geometry");
		}
		for (auto Index : Geometry.Indices)
		{
			if (Index >= Geometry.Vertices.size() / Geometry.VertexStride)
			{
				throw std::invalid_argument("Invalid geometry index");
			}
		}
		InRecord.Vertices.push_back(
		    Device.CreateBuffer({Geometry.Vertices.size(), BufferUsage(ERHIBufferUsage::Vertex)}, Geometry.Vertices));
		const auto IndexBytes = std::as_bytes(std::span(Geometry.Indices));
		InRecord.Indices.push_back(
		    Device.CreateBuffer({IndexBytes.size(), BufferUsage(ERHIBufferUsage::Index)}, IndexBytes));
		++Stats.GeometryUploads;
	}
	for (const auto& Section : Desc.Sections)
	{
		if (Section.Geometry >= Desc.Geometries.size() || Section.Material >= Desc.Materials.size() ||
		    std::uint64_t(Section.FirstIndex) + Section.IndexCount > Desc.Geometries[Section.Geometry].Indices.size())
		{
			throw std::invalid_argument("Invalid render section");
		}
	}
	for (const auto& Material : Desc.Materials)
	{
		InRecord.Materials.push_back(AcquireMaterial(Material.Surface, Material.Compiled));
	}
	InRecord.Publish(ERenderResourceStatus::Ready);
}

FRenderResourceStats FRenderResourceService::Statistics() const
{
	std::lock_guard Lock(Coordinator->Mutex);
	return Coordinator->Stats;
}

void FRenderResourceService::Close()
{
	auto& Owner = *Coordinator;
	FTaskHandle Progress;
	std::vector<FTaskHandle> Preparations;
	{
		std::lock_guard Lock(Owner.Mutex);
		if (Owner.bClosed && Owner.bNativeClosed)
		{
			return;
		}
		Owner.Tasks.Require({EDomain::Main});
		Owner.bClosed = true;
		Progress = Owner.Progress;
		for (const auto& [Key, Entry] : Owner.Entries)
		{
			Preparations.push_back(Entry.Record->Preparation.Task());
		}
		for (const auto& [Identity, Program] : Owner.Programs)
		{
			if (Program)
			{
				Preparations.push_back(Program->Preparation.Task());
			}
		}
	}
	try
	{
		Owner.Tasks.Wait(Progress);
	}
	catch (...)
	{
	}
	for (const auto& Task : Preparations)
	{
		try
		{
			Owner.Tasks.Wait(Task);
		}
		catch (...)
		{
		}
	}
	Owner.Tasks.Wait(Owner.Tasks.Dispatch({EDomain::Rhi, 0},
	                                      [State = Coordinator]
	                                      {
		                                      State->CloseNativeResources();
	                                      }));
}
} // namespace Hyperion
