#include "Hyperion/Renderer/MaterialPacking.h"
#include "Hyperion/Renderer/RenderBatch.h"
#include "Hyperion/Renderer/RenderMaterial.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
bool FInstanceBatchData::IsLive() const
{
	return !Owners.empty() && std::all_of(Owners.begin(), Owners.end(),
	                                      [](const auto& InOwner)
	                                      {
		                                      return !InOwner.expired();
	                                      });
}

std::size_t FInstanceBatchData::ByteSize() const
{
	std::size_t Result{};
	for (const auto& Constant : Constants)
	{
		Result += Constant.Bytes.size();
	}
	return Result;
}

std::shared_ptr<const FInstanceBatchData> PackInstanceBatch(const FRenderSceneSnapshot& InSnapshot,
                                                            std::span<const std::size_t> InItems)
{
	if (InItems.empty() || InItems.size() > UINT32_MAX)
	{
		throw std::invalid_argument("Invalid instance batch size");
	}
	auto Result = std::make_shared<FInstanceBatchData>();
	Result->InstanceCount = static_cast<std::uint32_t>(InItems.size());
	const auto& First = InSnapshot.Items.at(InItems.front());
	const auto Program = First.State.Surface->GetCompiled();
	const auto& Pass = Program->GetPass(InSnapshot.View.Usage, "Instance");
	for (std::uint32_t Slot = 0; Slot < Pass.Bindings.size(); ++Slot)
	{
		const auto& Binding = Pass.Bindings[Slot];
		if (!Binding.InstanceStride)
		{
			continue;
		}
		if (InItems.size() > Binding.InstanceCapacity)
		{
			throw std::invalid_argument("Instance batch exceeds shader capacity");
		}
		FInstanceConstantBlock Block{Slot};
		Block.Bytes.reserve(InItems.size() * Binding.InstanceStride);
		for (const auto Index : InItems)
		{
			const auto& Item = InSnapshot.Items.at(Index);
			const auto ItemProgram = Item.State.Surface->GetCompiled();
			const auto& ItemBinding = ItemProgram->GetPass(InSnapshot.View.Usage, "Instance").Bindings.at(Slot);
			if (ItemBinding.InstanceStride != Binding.InstanceStride)
			{
				throw std::invalid_argument("Incompatible instance record layout");
			}
			const auto Bytes = PackMaterialConstants(ItemBinding, Item.ResolvedParameters->Values);
			Block.Bytes.insert(Block.Bytes.end(), Bytes.begin(), Bytes.end());
		}
		Result->Constants.push_back(std::move(Block));
	}
	for (const auto Index : InItems)
	{
		const auto& Item = InSnapshot.Items.at(Index);
		Result->Owners.push_back(Item.Lifetime ? Item.Lifetime : Item.State.Resource);
	}
	return Result;
}
} // namespace Hyperion
