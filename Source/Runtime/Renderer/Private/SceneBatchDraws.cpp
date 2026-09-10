#include "Hyperion/Core/Profiling.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "SceneDrawsInternal.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
void FRenderResourceCoordinator::ValidateDrawItem(const FRenderItem& InItem, const FRenderView& InView)
{
	if (!InItem.PreparationError.empty())
	{
		throw std::runtime_error(InItem.PreparationError);
	}
	if (!InItem.State.Resource || !InItem.State.Surface)
	{
		throw std::invalid_argument("A draw requires ready geometry and a material selection");
	}
	const auto& Geometry = *InItem.State.Resource->Record;
	const auto& Material = *InItem.State.Surface->Record;
	if (Geometry.Owner != this || Material.Owner != this || Geometry.Status != ERenderResourceStatus::Ready ||
	    Material.Status != ERenderMaterialStatus::Ready)
	{
		throw std::invalid_argument("Unready or foreign geometry/material resource");
	}
	(void)Geometry.Description->Sections.at(InItem.State.Section);
	const auto& Pass = InItem.State.Surface->GetSnapshot()->Definition->GetPass(InView.Usage);
	if (InItem.DynamicState && !Pass.bAllowDynamicOverrides)
	{
		throw std::invalid_argument("Material pass does not allow dynamic draw-state overrides");
	}
	ValidateGraphicsDynamicState(ConvertMaterialDynamicState(InItem.DynamicState.value_or(Pass.DynamicState)));
}

void FRenderResourceCoordinator::BindInstances(FDrawPacket& InPacket, std::shared_ptr<const FInstanceBatchData> InData)
{
	const auto Bindings = MaterialConstants->BindInstances(InData);
	for (const auto& Binding : Bindings)
	{
		const auto Existing = std::find_if(InPacket.ConstantBindings.begin(), InPacket.ConstantBindings.end(),
		                                   [&](const auto& InBinding)
		                                   {
			                                   return InBinding.Slot == Binding.Slot;
		                                   });
		if (Existing == InPacket.ConstantBindings.end())
		{
			throw std::invalid_argument("Instance constant slot is absent from prepared draw");
		}
		*Existing = Binding;
	}
	InPacket.InstanceCount = InData->InstanceCount;
}

namespace
{
auto GroupKey(const FRenderItem& InItem)
{
	return std::pair{InItem.Primitive.Scene, InItem.Group};
}

struct FPendingBatch
{
	std::vector<std::size_t> Items;
	FDrawPacket Packet;
};

std::vector<std::size_t> Survivors(const FRenderSceneSnapshot& InSnapshot, std::span<const std::size_t> InItems,
                                   const FPreparedSceneDraws& InPrepared)
{
	std::vector<std::size_t> Result;
	for (const auto Index : InItems)
	{
		if (!InPrepared.Failures.contains(GroupKey(InSnapshot.Items[Index])))
		{
			Result.push_back(Index);
		}
	}
	return Result;
}

void ValidateBatch(FRenderResourceCoordinator& InOwner, const FRenderSceneSnapshot& InSnapshot,
                   const FRenderBatch& InBatch, FPreparedSceneDraws& OutPrepared)
{
	const auto Revision = InOwner.PublicationRevision.load(std::memory_order_acquire);
	const auto Existing = InOwner.BatchAdmissions.find(InBatch.LocalContentIdentity.get());
	if (InBatch.LocalContentIdentity && Existing != InOwner.BatchAdmissions.end() &&
	    Existing->second.Contents.lock() == InBatch.LocalContentIdentity &&
	    Existing->second.ResourceRevision == Revision && Existing->second.Usage == InSnapshot.View.Usage)
	{
		for (const auto Index : InBatch.Items)
		{
			OutPrepared.Srgb[Index] = Existing->second.bSrgb;
		}
		++OutPrepared.Statistics.BatchAdmissionReuses;
		return;
	}
	bool bReady = !InBatch.Items.empty();
	bool bSrgb{};
	for (const auto Index : InBatch.Items)
	{
		const auto& Item = InSnapshot.Items[Index];
		try
		{
			InOwner.ValidateDrawItem(Item, InSnapshot.View);
			const bool bCurrentSrgb =
			    Item.State.Surface->GetSnapshot()->Definition->GetPass(InSnapshot.View.Usage).bSrgbTarget;
			bReady &= Index == InBatch.Items.front() || bSrgb == bCurrentSrgb;
			bSrgb = bCurrentSrgb;
			OutPrepared.Srgb[Index] = bSrgb;
		}
		catch (const std::exception& Error)
		{
			bReady = false;
			OutPrepared.Failures[GroupKey(Item)] = Error.what();
		}
	}
	if (!bReady || !InBatch.LocalContentIdentity)
	{
		return;
	}
	if (InOwner.BatchAdmissions.size() >= 1024)
	{
		std::erase_if(InOwner.BatchAdmissions,
		              [&](const auto& InEntry)
		              {
			              return InEntry.second.Contents.expired() || InEntry.second.ResourceRevision != Revision;
		              });
	}
	if (InOwner.BatchAdmissions.size() < 1024)
	{
		InOwner.BatchAdmissions.insert_or_assign(
		    InBatch.LocalContentIdentity.get(),
		    FRenderResourceCoordinator::FBatchAdmission{InBatch.LocalContentIdentity, Revision, InSnapshot.View.Usage,
		                                                bSrgb});
	}
}

void PrepareSingles(FRenderResourceCoordinator& InOwner, const FRenderSceneSnapshot& InSnapshot,
                    std::span<const std::size_t> InItems, FPreparedSceneDraws& OutPrepared)
{
	for (const auto Index : InItems)
	{
		const auto& Item = InSnapshot.Items[Index];
		try
		{
			OutPrepared.Packets[Index] =
			    InOwner.DrawMaterial(Item, InSnapshot.View, {OutPrepared.Srgb[Index], OutPrepared.Depth});
		}
		catch (const std::exception& Error)
		{
			OutPrepared.Failures[GroupKey(Item)] = Error.what();
		}
	}
}

void RepairBatches(FRenderResourceCoordinator& InOwner, const FRenderSceneSnapshot& InSnapshot,
                   std::vector<FPendingBatch>& InPending, FPreparedSceneDraws& OutPrepared)
{
	// A later section can fail after an earlier section entered a cross-model batch. Publish only after repair.
	bool bChanged;
	do
	{
		bChanged = false;
		for (auto& Batch : InPending)
		{
			auto Items = Survivors(InSnapshot, Batch.Items, OutPrepared);
			if (Items == Batch.Items)
			{
				continue;
			}
			Batch.Items = std::move(Items);
			if (Batch.Items.empty())
			{
				continue;
			}
			try
			{
				const auto Data = PackInstanceBatch(InSnapshot, Batch.Items);
				InOwner.BindInstances(Batch.Packet, Data);
				OutPrepared.Statistics.PackedBytes += Data->ByteSize();
				++OutPrepared.Statistics.RebuiltChunks;
			}
			catch (const std::exception& Error)
			{
				for (const auto Index : Batch.Items)
				{
					OutPrepared.Failures[GroupKey(InSnapshot.Items[Index])] = Error.what();
				}
				Batch.Items.clear();
				bChanged = true;
			}
		}
	} while (bChanged);
	for (auto& Batch : InPending)
	{
		if (!Batch.Items.empty())
		{
			OutPrepared.Packets[Batch.Items.front()] = std::move(Batch.Packet);
		}
	}
}
} // namespace

void PrepareBatchedDraws(FRenderResourceCoordinator& InOwner, const FRenderSceneSnapshot& InSnapshot,
                         FPreparedSceneDraws& OutPrepared)
{
	HYP_PERF_SCOPE_C(Detail, PrepareBatchedDrawPackets);
	OutPrepared.Statistics = InSnapshot.Batches->Statistics;
	for (const auto& Batch : InSnapshot.Batches->Batches)
	{
		ValidateBatch(InOwner, InSnapshot, Batch, OutPrepared);
	}
	std::vector<FPendingBatch> Pending;
	for (const auto& Batch : InSnapshot.Batches->Batches)
	{
		auto Items = Survivors(InSnapshot, Batch.Items, OutPrepared);
		if (Items.empty())
		{
			continue;
		}
		if (Items.size() < 2 || !Batch.Instances)
		{
			PrepareSingles(InOwner, InSnapshot, Items, OutPrepared);
			continue;
		}
		try
		{
			const auto Index = Items.front();
			auto Packet = InOwner.DrawMaterial(InSnapshot.Items[Index], InSnapshot.View,
			                                   {OutPrepared.Srgb[Index], OutPrepared.Depth}, true);
			const auto Data = Items == Batch.Items ? Batch.Instances : PackInstanceBatch(InSnapshot, Items);
			if (Items != Batch.Items)
			{
				OutPrepared.Statistics.PackedBytes += Data->ByteSize();
				++OutPrepared.Statistics.RebuiltChunks;
			}
			InOwner.BindInstances(Packet, Data);
			Pending.push_back({std::move(Items), std::move(Packet)});
		}
		catch (const std::exception&)
		{
			OutPrepared.Statistics.Fallbacks[static_cast<std::size_t>(ERenderBatchFallback::Preparation)] +=
			    Items.size();
			PrepareSingles(InOwner, InSnapshot, Items, OutPrepared);
		}
	}
	RepairBatches(InOwner, InSnapshot, Pending, OutPrepared);
}
} // namespace Hyperion
