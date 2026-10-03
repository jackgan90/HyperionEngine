#include "ComputeResources.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include "Hyperion/Renderer/MaterialBlocks.h"
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
	auto Options = InPass.Shader.Options;
	for (auto& Include : GenerateShaderIncludes(InPass.Shader.Contracts))
	{
		Options.VirtualIncludes.push_back({std::move(Include.first), std::move(Include.second)});
	}
	Program.Shader =
	    InCompiler.Compile(InPass.Shader.Source, InPass.Shader.Entry, EShaderStage::Compute, InFormat, Options);
	for (const auto& Resource : Program.Shader.Bindings)
	{
		auto Validated = Resource;
		NormalizeStandardMaterialBlock(Validated, InPass.Shader.Contracts);
		ValidateEngineMaterialResource(Validated, InPass.Shader.Contracts);
		FMaterialProgramBinding Binding;
		Binding.Resource = Validated;
		Binding.Stages = EShaderStageMask::Compute;
		if (Resource.Kind == EBindingKind::UniformBuffer)
		{
			for (const auto& Member : Validated.Members)
			{
				if (Member.bActive)
				{
					Binding.Members.push_back({Member, Binding.Members.size()});
				}
			}
		}
		std::vector<FMaterialSemanticId> Members;
		const auto Contract = GetStandardMaterialBlock(Resource.Name, InPass.Shader.Contracts);
		for (const auto& Member : Binding.Members)
		{
			const auto Expected = std::find_if(Contract.Members.begin(), Contract.Members.end(),
			                                   [&](const auto& InMember)
			                                   {
				                                   return InMember.Name == Member.Layout.Name;
			                                   });
			Members.push_back(Expected == Contract.Members.end() ? FMaterialSemanticId{} : Expected->Semantic);
		}
		Program.MemberSemantics.push_back(std::move(Members));
		Program.ResourceSemantics.push_back(GetEngineMaterialResource(Resource.Name, InPass.Shader.Contracts).Semantic);
		Program.ActiveSemantics.insert(Program.MemberSemantics.back().begin(), Program.MemberSemantics.back().end());
		Program.ActiveSemantics.insert(Program.ResourceSemantics.back());
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
bool IsKnownComputeSemantic(FMaterialSemanticId InSemantic, const FShaderParameterContracts& InContracts)
{
	if (InSemantic.GetBuiltin() != EEngineSemantic::None)
	{
		return true;
	}
	for (const auto Sets :
	     {std::span<const std::shared_ptr<const FShaderParameterContractSet>>(GetStandardShaderContracts()),
	      std::span<const std::shared_ptr<const FShaderParameterContractSet>>(InContracts)})
	{
		for (const auto& Set : Sets)
		{
			if (std::find(Set->Semantics.begin(), Set->Semantics.end(), InSemantic.GetDescriptor()) !=
			    Set->Semantics.end())
			{
				return true;
			}
		}
	}
	return false;
}

template<typename T>
void SelectComputeResources(std::vector<T>& InResources, const FComputePassDesc& InPass,
                            const FComputeResources::FProgram& InProgram)
{
	std::set<std::pair<FMaterialSemanticId, std::uint32_t>> Seen;
	std::erase_if(InResources,
	              [&](const T& InParameter)
	              {
		              if (InParameter.Semantic.IsEmpty())
		              {
			              return false;
		              }
		              if (!InParameter.Semantic.IsBuiltin() ||
		                  !IsKnownComputeSemantic(InParameter.Semantic, InPass.Shader.Contracts) ||
		                  !Seen.emplace(InParameter.Semantic, InParameter.ArrayIndex).second)
		              {
			              throw std::invalid_argument("Unknown or duplicate compute resource semantic");
		              }
		              const auto Kind = GetStandardMaterialSemantics()->Find(InParameter.Semantic).Type.Kind;
		              const bool bCorrectKind = std::is_same_v<T, FComputeTextureParameter>
		                                            ? IsMaterialTexture(Kind)
		                                            : Kind == EMaterialValueKind::ReadBuffer;
		              if (!bCorrectKind)
		              {
			              throw std::invalid_argument("Compute resource semantic kind mismatch");
		              }
		              return !InProgram.ActiveSemantics.contains(InParameter.Semantic);
	              });
}

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

const FMaterialValue* EngineParameter(const FComputePassDesc& InPass, FMaterialSemanticId InSemantic,
                                      std::set<std::size_t>& InUsed)
{
	if (InSemantic.IsEmpty())
	{
		return nullptr;
	}
	for (std::size_t Index = 0; Index < InPass.EngineParameters.size(); ++Index)
	{
		if (InPass.EngineParameters[Index].Semantic == InSemantic)
		{
			InUsed.insert(Index);
			return &InPass.EngineParameters[Index].Value;
		}
	}
	return nullptr;
}

FBufferSlice Constants(FRenderResourceCoordinator& InOwner, const FMaterialProgramBinding& InBinding,
                       const FComputePassDesc& InPass, std::set<std::string>& InUsed,
                       std::set<std::size_t>& InUsedEngine, const std::vector<FMaterialSemanticId>& InSemantics)
{
	std::vector<std::optional<FMaterialValue>> Values;
	for (std::size_t Index = 0; Index < InBinding.Members.size(); ++Index)
	{
		const auto& Member = InBinding.Members[Index];
		const auto* Input = EngineParameter(InPass, InSemantics[Index], InUsedEngine);
		if (Input)
		{
			Values.push_back(*Input);
		}
		else
		{
			Values.push_back(Parameter(InPass, InBinding.Resource.Name + "." + Member.Layout.Name, InUsed));
		}
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
                              const FComputePassDesc& InPass, std::uint32_t InIndex, std::set<std::size_t>& InUsed,
                              FMaterialSemanticId InSemantic)
{
	for (std::size_t Index = 0; Index < InPass.Textures.size(); ++Index)
	{
		const auto& Parameter = InPass.Textures[Index];
		if ((Parameter.Semantic.IsEmpty() ? Parameter.Name != InBinding.Name : Parameter.Semantic != InSemantic) ||
		    Parameter.ArrayIndex != InIndex)
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
                             const FComputePassDesc& InPass, std::uint32_t InIndex, std::set<std::size_t>& InUsed,
                             FMaterialSemanticId InSemantic)
{
	for (std::size_t Index = 0; Index < InPass.Buffers.size(); ++Index)
	{
		const auto& Parameter = InPass.Buffers[Index];
		if ((Parameter.Semantic.IsEmpty() ? Parameter.Name != InBinding.Name : Parameter.Semantic != InSemantic) ||
		    Parameter.ArrayIndex != InIndex)
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

FComputePassDesc SelectComputeParameters(const FComputePassDesc& InPass, const FComputeResources::FProgram& InProgram)
{
	auto Result = InPass;
	std::set<FMaterialSemanticId> Seen;
	std::erase_if(Result.EngineParameters,
	              [&](const auto& InEntry)
	              {
		              if (!InEntry.Semantic.IsBuiltin() ||
		                  !IsKnownComputeSemantic(InEntry.Semantic, InPass.Shader.Contracts) ||
		                  !Seen.insert(InEntry.Semantic).second ||
		                  InEntry.Value.Type != GetStandardMaterialSemantics()->Find(InEntry.Semantic).Type)
		              {
			              throw std::invalid_argument("Invalid or duplicate compute engine semantic");
		              }
		              InEntry.Value.Validate();
		              return !InProgram.ActiveSemantics.contains(InEntry.Semantic);
	              });
	SelectComputeResources(Result.Textures, InPass, InProgram);
	SelectComputeResources(Result.Buffers, InPass, InProgram);
	return Result;
}

FDispatchPacket FRenderResourcePreparation::BuildCompute(const FComputePassDesc& InDescription) const
{
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::lock_guard Lock(Owner.Mutex);
	if (Owner.bClosed || !InDescription.Lifetime)
	{
		throw std::logic_error("Compute preparation requires a live resource scope");
	}
	Owner.EnsureMaterialCaches();
	Owner.TrackScope(InDescription.Lifetime);
	auto& Program =
	    Owner.Compute.GetProgram(Owner.Compiler, Owner.Device.GetCapabilities().ShaderFormat, InDescription);
	const auto InPass = SelectComputeParameters(InDescription, Program);
	FDispatchPacket Dispatch;
	FResourceBindingSetDesc Set;
	Set.Layout = Owner.MaterialGpu->GetLayout(DescribeShaderLayout(Program.Bindings), {InPass.Lifetime});
	std::set<std::string> Used;
	std::set<std::size_t> UsedTextures;
	std::set<std::size_t> UsedBuffers;
	std::set<std::size_t> UsedEngine;
	for (std::uint32_t Slot = 0; Slot < Program.Bindings.size(); ++Slot)
	{
		const auto& Binding = Program.Bindings[Slot];
		if (Binding.Resource.Kind == EBindingKind::UniformBuffer)
		{
			Dispatch.ConstantBindings.push_back(
			    {Slot, Constants(Owner, Binding, InPass, Used, UsedEngine, Program.MemberSemantics[Slot])});
			continue;
		}
		FResourceBindingEntry Entry;
		Entry.Slot = Slot;
		for (std::uint32_t Index = 0; Index < Binding.Resource.Count; ++Index)
		{
			if (Binding.Resource.Kind == EBindingKind::Texture || Binding.Resource.Kind == EBindingKind::StorageTexture)
			{
				Entry.Values.push_back(
				    Texture(Owner, Binding.Resource, InPass, Index, UsedTextures, Program.ResourceSemantics[Slot]));
			}
			else if (Binding.Resource.Kind == EBindingKind::Sampler)
			{
				const auto* Engine = EngineParameter(InPass, Program.ResourceSemantics[Slot], UsedEngine);
				const auto& Value = Engine ? *Engine : Parameter(InPass, Binding.Resource.Name, Used);
				if (Binding.Resource.Count > 1 && Value.Elements.size() != Binding.Resource.Count)
				{
					throw std::invalid_argument("Compute sampler array size mismatch");
				}
				Entry.Values.push_back(Owner.MaterialGpu->GetResource(
				    Binding.Resource.Count == 1 ? Value : Value.Elements[Index], {InPass.Lifetime}));
			}
			else
			{
				Entry.Values.push_back(
				    Buffer(Owner, Binding.Resource, InPass, Index, UsedBuffers, Program.ResourceSemantics[Slot]));
			}
		}
		Set.Entries.push_back(std::move(Entry));
	}
	if (Used.size() != InPass.Parameters.size() || UsedTextures.size() != InPass.Textures.size() ||
	    UsedBuffers.size() != InPass.Buffers.size() || UsedEngine.size() != InPass.EngineParameters.size())
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
