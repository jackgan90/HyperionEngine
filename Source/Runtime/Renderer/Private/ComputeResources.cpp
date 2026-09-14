#include "ComputeResources.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include "Hyperion/Renderer/MaterialPacking.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "RenderResourcesInternal.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
FComputeResources::FProgram& FComputeResources::GetProgram(FShaderCompiler& InCompiler, EShaderFormat InFormat,
                                                           const FComputePassDesc& InPass)
{
	for (auto& Program : Programs)
	{
		if (Program.Description == InPass.Shader && Program.Shader.Format == InFormat)
		{
			if (std::none_of(Program.Owners.begin(), Program.Owners.end(),
			                 [&](const auto& InOwner)
			                 {
				                 return InOwner.lock() == InPass.Lifetime;
			                 }))
			{
				Program.Owners.push_back(InPass.Lifetime);
			}
			return Program;
		}
	}
	FProgram Program;
	Program.Description = InPass.Shader;
	Program.Shader = InCompiler.Compile(InPass.Shader.Source, InPass.Shader.Entry, EShaderStage::Compute, InFormat,
	                                    InPass.Shader.Options);
	for (const auto& Resource : Program.Shader.Bindings)
	{
		FMaterialProgramBinding Binding;
		Binding.Resource = Resource;
		Binding.Stages = 4;
		for (const auto& Member : Resource.Members)
		{
			if (Member.bActive)
			{
				Binding.Members.push_back({Member, Binding.Members.size()});
			}
		}
		Program.Bindings.push_back(std::move(Binding));
	}
	Program.Owners.push_back(InPass.Lifetime);
	Programs.push_back(std::move(Program));
	return Programs.back();
}

void FComputeResources::Collect()
{
	std::erase_if(Programs,
	              [](auto& InProgram)
	              {
		              std::erase_if(InProgram.Owners,
		                            [](const auto& InOwner)
		                            {
			                            return InOwner.expired();
		                            });
		              return InProgram.Owners.empty();
	              });
	std::erase_if(Constants,
	              [](const auto& InConstant)
	              {
		              return InConstant.Owner.expired();
	              });
}

namespace
{
const FMaterialValue& Parameter(const FComputePassDesc& InPass, const std::string& InName,
                                std::set<std::string>& InUsed)
{
	const auto Found = std::find_if(InPass.Parameters.begin(), InPass.Parameters.end(),
	                                [&](const auto& InValue)
	                                {
		                                return InValue.first == InName;
	                                });
	if (Found == InPass.Parameters.end())
	{
		throw std::invalid_argument("Missing compute parameter: " + InName);
	}
	InUsed.insert(InName);
	return Found->second;
}

FBufferSlice Constants(FRenderResourceCoordinator& InOwner, const FMaterialProgramBinding& InBinding,
                       const FComputePassDesc& InPass, std::set<std::string>& InUsed)
{
	std::vector<std::optional<FMaterialValue>> Values;
	for (const auto& Member : InBinding.Members)
	{
		Values.push_back(Parameter(InPass, InBinding.Resource.Name + "." + Member.Layout.Name, InUsed));
	}
	auto Bytes = PackMaterialConstants(InBinding, Values);
	for (auto& Entry : InOwner.Compute.Constants)
	{
		if (Entry.Bytes == Bytes && Entry.Owner.lock() == InPass.Lifetime)
		{
			return Entry.Slice;
		}
	}
	auto Slice = InOwner.MaterialConstants->PublishPacked(Bytes);
	// Bounded value history. Eviction drops only this CPU reuse reference, never an in-flight slice.
	if (InOwner.Compute.Constants.size() >= 256)
	{
		InOwner.Compute.Constants.erase(InOwner.Compute.Constants.begin());
	}
	InOwner.Compute.Constants.push_back({std::move(Bytes), Slice, InPass.Lifetime});
	return Slice;
}

FResourceBindingValue Texture(FRenderResourceCoordinator& InOwner, const FShaderBinding& InBinding,
                              const FComputePassDesc& InPass, std::uint32_t InIndex, std::set<std::size_t>& InUsed)
{
	for (std::size_t Index = 0; Index < InPass.Textures.size(); ++Index)
	{
		const auto& Parameter = InPass.Textures[Index];
		if (Parameter.Name != InBinding.Name || Parameter.ArrayIndex != InIndex)
		{
			continue;
		}
		const bool bStorage = InBinding.Kind == EBindingKind::StorageTexture;
		if (bStorage != (Parameter.Access == EResourceState::ShaderWrite))
		{
			throw std::invalid_argument("Compute texture access differs from shader reflection");
		}
		InUsed.insert(Index);
		auto Texture = InOwner.MaterialGpu->GetTexture(Parameter.Source, {InPass.Lifetime});
		if (!InOwner.Device.TexturesReady(std::span(&Texture, 1)))
		{
			throw std::runtime_error("Compute texture upload is not ready");
		}
		if (Parameter.Source->GetDimension() == ETextureDimension::Cube)
		{
			if (Parameter.FirstMip || Parameter.MipCount != Parameter.Source->GetMips().size())
			{
				throw std::invalid_argument("Cube compute reads require the complete mip chain");
			}
			return Texture;
		}
		return FTextureView{std::move(Texture), Parameter.FirstMip, Parameter.MipCount};
	}
	throw std::invalid_argument("Missing compute texture: " + InBinding.Name);
}

FResourceBindingValue Buffer(FRenderResourceCoordinator& InOwner, const FShaderBinding& InBinding,
                             const FComputePassDesc& InPass, std::uint32_t InIndex, std::set<std::size_t>& InUsed)
{
	for (std::size_t Index = 0; Index < InPass.Buffers.size(); ++Index)
	{
		const auto& Parameter = InPass.Buffers[Index];
		if (Parameter.Name != InBinding.Name || Parameter.ArrayIndex != InIndex)
		{
			continue;
		}
		const bool bStorage =
		    InBinding.Kind == EBindingKind::StorageStructuredBuffer || InBinding.Kind == EBindingKind::StorageRawBuffer;
		if (bStorage != (Parameter.Access == EResourceState::ShaderWrite))
		{
			throw std::invalid_argument("Compute buffer access differs from shader reflection");
		}
		InUsed.insert(Index);
		return InOwner.MaterialGpu->GetResource(FMaterialValue::FromBuffer(Parameter.View), {InPass.Lifetime});
	}
	throw std::invalid_argument("Missing compute buffer: " + InBinding.Name);
}
} // namespace

FDispatchPacket FRenderResourcePreparation::BuildCompute(const FComputePassDesc& InPass) const
{
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::lock_guard Lock(Owner.Mutex);
	if (Owner.bClosed || !InPass.Lifetime)
	{
		throw std::logic_error("Compute preparation requires a live resource scope");
	}
	Owner.EnsureMaterialCaches();
	Owner.TrackScope(InPass.Lifetime);
	auto& Program = Owner.Compute.GetProgram(Owner.Compiler, Owner.Device.GetCapabilities().ShaderFormat, InPass);
	FDispatchPacket Dispatch;
	FResourceBindingSetDesc Set;
	Set.Layout = Owner.MaterialGpu->GetLayout(DescribeShaderLayout(Program.Bindings), {InPass.Lifetime});
	std::set<std::string> Used;
	std::set<std::size_t> UsedTextures;
	std::set<std::size_t> UsedBuffers;
	for (std::uint32_t Slot = 0; Slot < Program.Bindings.size(); ++Slot)
	{
		const auto& Binding = Program.Bindings[Slot];
		if (Binding.Resource.Kind == EBindingKind::UniformBuffer)
		{
			Dispatch.ConstantBindings.push_back({Slot, Constants(Owner, Binding, InPass, Used)});
			continue;
		}
		FResourceBindingEntry Entry;
		Entry.Slot = Slot;
		for (std::uint32_t Index = 0; Index < Binding.Resource.Count; ++Index)
		{
			if (Binding.Resource.Kind == EBindingKind::Texture || Binding.Resource.Kind == EBindingKind::StorageTexture)
			{
				Entry.Values.push_back(Texture(Owner, Binding.Resource, InPass, Index, UsedTextures));
			}
			else if (Binding.Resource.Kind == EBindingKind::Sampler)
			{
				const auto& Value = Parameter(InPass, Binding.Resource.Name, Used);
				if (Binding.Resource.Count > 1 && Value.Elements.size() != Binding.Resource.Count)
				{
					throw std::invalid_argument("Compute sampler array size mismatch");
				}
				Entry.Values.push_back(Owner.MaterialGpu->GetResource(
				    Binding.Resource.Count == 1 ? Value : Value.Elements[Index], {InPass.Lifetime}));
			}
			else
			{
				Entry.Values.push_back(Buffer(Owner, Binding.Resource, InPass, Index, UsedBuffers));
			}
		}
		Set.Entries.push_back(std::move(Entry));
	}
	if (Used.size() != InPass.Parameters.size() || UsedTextures.size() != InPass.Textures.size() ||
	    UsedBuffers.size() != InPass.Buffers.size())
	{
		throw std::invalid_argument("Unknown, inactive or duplicate compute parameter");
	}
	Dispatch.Pipeline = Owner.MaterialGpu->GetComputePipeline({Program.Shader, Set.Layout}, {InPass.Lifetime});
	Dispatch.Bindings = Owner.MaterialGpu->GetSet(std::move(Set), {InPass.Lifetime});
	Dispatch.Groups = ComputeDispatchGroups(InPass.Extent, Program.Shader.Reflection.ThreadGroupSize);
	if (InPass.Statistics)
	{
		++InPass.Statistics->Dispatches;
		InPass.Statistics->Groups = Dispatch.Groups;
	}
	return Dispatch;
}
} // namespace Hyperion
