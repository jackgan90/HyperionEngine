#include "Hyperion/Renderer/RenderBatch.h"
#include "InstancePacking.h"
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
		Result += Constant.Bytes ? Constant.Bytes->size() : 0;
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
	const auto& First = InSnapshot.Items.At(InItems.front());
	const auto Program = GetInstanceProgram(First);
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
		const auto Layout = DescribeInstanceRecordLayout(*Program, Pass, Binding);
		auto Packed = std::make_shared<std::vector<std::byte>>();
		Packed->reserve(InItems.size() * Binding.InstanceStride);
		for (const auto Index : InItems)
		{
			const auto& Item = InSnapshot.Items.At(Index);
			const auto ItemProgram = GetInstanceProgram(Item);
			const auto& ItemPass = ItemProgram->GetPass(InSnapshot.View.Usage, "Instance");
			const auto& ItemBinding = ItemPass.Bindings.at(Slot);
			if (ItemProgram != Program &&
			    !Layout.Matches(DescribeInstanceRecordLayout(*ItemProgram, ItemPass, ItemBinding)))
			{
				throw std::invalid_argument("Incompatible instance record layout");
			}
			const auto Bytes = PackInstanceRecord(Item, ItemBinding);
			Packed->insert(Packed->end(), Bytes.begin(), Bytes.end());
		}
		Block.Bytes = std::move(Packed);
		Result->Constants.push_back(std::move(Block));
	}
	for (const auto Index : InItems)
	{
		const auto& Item = InSnapshot.Items.At(Index);
		Result->Owners.push_back(Item.Lifetime ? Item.Lifetime : Item.State.Resource);
	}
	return Result;
}
} // namespace Hyperion
