#include "Hyperion/Renderer/MaterialPipeline.h"
#include "MaterialGpuCacheInternal.h"

namespace Hyperion
{
namespace
{
std::size_t LayoutHash(const FResourceBindingLayoutDesc& InDescription)
{
	std::size_t Hash = InDescription.Slots.size();
	for (const auto& Slot : InDescription.Slots)
	{
		Hash = Hash * 16777619U ^ (Slot.Register + Slot.Space * 1000U + Slot.Count * 4000U +
		                           static_cast<unsigned>(Slot.Kind) * 100000U + static_cast<unsigned>(Slot.Visibility));
		Hash = Hash * 16777619U ^ Slot.StructureByteStride;
	}
	return Hash;
}

std::size_t SetHash(const FResourceBindingSetDesc& InDescription)
{
	std::size_t Hash = std::hash<const void*>{}(InDescription.Layout.Payload.get());
	for (const auto& Entry : InDescription.Entries)
	{
		Hash = Hash * 16777619U ^ Entry.Slot;
		for (const auto& Value : Entry.Values)
		{
			const auto Pointer = std::visit(
			    [](const auto& InValue) -> const void*
			    {
				    if constexpr (std::is_same_v<std::decay_t<decltype(InValue)>, FReadBufferView>)
				    {
					    return InValue.Buffer.Payload.get();
				    }
				    else
				    {
					    return InValue.Payload.get();
				    }
			    },
			    Value);
			Hash = Hash * 16777619U ^ std::hash<const void*>{}(Pointer);
		}
	}
	return Hash;
}

bool SamePipeline(const FPipelineDesc& InA, const FPipelineDesc& InB)
{
	return InA.Vertex.Format == InB.Vertex.Format && InA.Pixel.Format == InB.Pixel.Format &&
	       InA.Vertex.Bytes == InB.Vertex.Bytes && InA.Pixel.Bytes == InB.Pixel.Bytes && InA.Layout == InB.Layout &&
	       InA.Attributes == InB.Attributes && InA.VertexStride == InB.VertexStride && InA.Topology == InB.Topology &&
	       InA.State == InB.State && InA.Target == InB.Target;
}
} // namespace

FMaterialGpuCache::FMaterialGpuCache(IRHIDevice& InDevice) : Impl(std::make_unique<FImpl>(InDevice))
{
}

FMaterialGpuCache::~FMaterialGpuCache() = default;

void FMaterialGpuCache::FImpl::CheckOwner() const
{
	if (Owner != std::this_thread::get_id())
	{
		throw std::logic_error("Material GPU cache requires its RHI owner");
	}
}

FResourceBindingLayout FMaterialGpuCache::GetLayout(const FResourceBindingLayoutDesc& InDescription,
                                                    const FMaterialResourceOwners& InOwners)
{
	Impl->CheckOwner();
	const auto Hash = LayoutHash(InDescription);
	const auto [Begin, End] = Impl->Layouts.equal_range(Hash);
	for (auto Iterator = Begin; Iterator != End; ++Iterator)
	{
		auto& Entry = Iterator->second;
		if (Entry.Description == InDescription)
		{
			Entry.Ownership.Add(InOwners);
			return Entry.Resource;
		}
	}
	FImpl::FLayoutEntry Entry;
	Entry.Description = InDescription;
	Entry.Ownership.Add(InOwners);
	Entry.Resource = Impl->Device.CreateBindingLayout(InDescription);
	const auto Iterator = Impl->Layouts.emplace(Hash, std::move(Entry));
	++Impl->Stats.LayoutsCreated;
	return Iterator->second.Resource;
}

FResourceBindingSet FMaterialGpuCache::FImpl::Set(FResourceBindingSetDesc InDescription,
                                                  const FMaterialResourceOwners& InOwners)
{
	const auto Hash = SetHash(InDescription);
	const auto [Begin, End] = Sets.equal_range(Hash);
	for (auto Iterator = Begin; Iterator != End; ++Iterator)
	{
		auto& Entry = Iterator->second;
		if (Entry.Description == InDescription)
		{
			Entry.Ownership.Add(InOwners);
			++Stats.SetReuses;
			return Entry.Resource;
		}
	}
	FSetEntry Entry;
	Entry.Description = std::move(InDescription);
	Entry.Ownership.Add(InOwners);
	Entry.Resource = Device.CreateBindingSet(Entry.Description);
	const auto Iterator = Sets.emplace(Hash, std::move(Entry));
	++Stats.SetsCreated;
	return Iterator->second.Resource;
}

FMaterialResourceBindings FMaterialGpuCache::BindResourceValues(const FCompiledMaterialPass& InPass,
                                                                std::span<const FMaterialValue* const> InValues,
                                                                const FMaterialResourceOwners& InOwners,
                                                                bool bInAllowMissing)
{
	Impl->CheckOwner();
	FResourceBindingSetDesc Description;
	Description.Layout = GetLayout(DescribeMaterialLayout(InPass), InOwners);
	std::vector<FTexture> Textures;
	bool bComplete = true;
	for (std::uint32_t Index = 0; Index < InPass.Bindings.size(); ++Index)
	{
		const auto& Binding = InPass.Bindings[Index];
		if (Binding.Resource.Kind == EBindingKind::UniformBuffer)
		{
			continue;
		}
		if (!Binding.ResourceParameter || *Binding.ResourceParameter >= InValues.size() ||
		    !InValues[*Binding.ResourceParameter])
		{
			if (bInAllowMissing)
			{
				bComplete = false;
				continue;
			}
			throw std::invalid_argument("Missing material resource: " + Binding.Resource.Name);
		}
		const auto& Value = *InValues[*Binding.ResourceParameter];
		Value.Validate();
		FResourceBindingEntry Entry;
		Entry.Slot = Index;
		if (Binding.Resource.Count == 1)
		{
			Entry.Values.push_back(Impl->Resource(Value, InOwners));
		}
		else
		{
			if (Value.Type.Kind != EMaterialValueKind::Array || Value.Elements.size() != Binding.Resource.Count)
			{
				throw std::invalid_argument("Material resource array has the wrong shape");
			}
			for (const auto& Element : Value.Elements)
			{
				Entry.Values.push_back(Impl->Resource(Element, InOwners));
			}
		}
		for (const auto& Native : Entry.Values)
		{
			if (const auto* Texture = std::get_if<FTexture>(&Native))
			{
				Textures.push_back(*Texture);
			}
		}
		Description.Entries.push_back(std::move(Entry));
	}
	FMaterialResourceBindings Result;
	Result.Layout = Description.Layout;
	if (bComplete)
	{
		Result.Set = Impl->Set(std::move(Description), InOwners);
	}
	Result.bReady = Textures.empty() || Impl->Device.TexturesReady(Textures);
	return Result;
}

FMaterialResourceBindings FMaterialGpuCache::BindResources(const FCompiledMaterialPass& InPass,
                                                           std::span<const std::optional<FMaterialValue>> InValues,
                                                           const FMaterialResourceOwners& InOwners,
                                                           bool bInAllowMissing)
{
	std::vector<const FMaterialValue*> Values;
	Values.reserve(InValues.size());
	for (const auto& Value : InValues)
	{
		Values.push_back(Value ? &*Value : nullptr);
	}
	return BindResourceValues(InPass, Values, InOwners, bInAllowMissing);
}

FMaterialResourceBindings FMaterialGpuCache::BindResources(
    const FCompiledMaterialPass& InPass, std::span<const std::shared_ptr<const FMaterialValue>> InValues,
    const FMaterialResourceOwners& InOwners, bool bInAllowMissing)
{
	std::vector<const FMaterialValue*> Values;
	Values.reserve(InValues.size());
	for (const auto& Value : InValues)
	{
		Values.push_back(Value ? &*Value : nullptr);
	}
	return BindResourceValues(InPass, Values, InOwners, bInAllowMissing);
}

FPipeline FMaterialGpuCache::GetMaterialPipeline(const FCompiledMaterialPass& InProgram, const FMaterialPass& InPass,
                                                 const FResourceBindingLayout& InLayout,
                                                 const std::vector<FVertexAttribute>& InAttributes,
                                                 std::uint32_t InStride, ERHIPrimitiveTopology InTopology,
                                                 FGraphicsTarget InTarget, bool bInMirrored,
                                                 const FMaterialResourceOwners& InOwners)
{
	Impl->CheckOwner();
	const auto State = ConvertMaterialState(InPass.State, bInMirrored);
	const auto Hash = std::hash<std::string>{}(InProgram.Vertex.CacheKey + "/" + InProgram.Pixel.CacheKey);
	const auto [Begin, End] = Impl->Pipelines.equal_range(Hash);
	for (auto It = Begin; It != End; ++It)
	{
		auto& Entry = It->second;
		const auto& Existing = Entry.Description;
		if (Existing.Layout == InLayout && Existing.Attributes == InAttributes && Existing.VertexStride == InStride &&
		    Existing.Topology == InTopology && Existing.Target == InTarget && InTarget.bSrgb == InPass.bSrgbTarget &&
		    Existing.State == State && Existing.Vertex.Format == InProgram.Vertex.Format &&
		    Existing.Pixel.Format == InProgram.Pixel.Format && Existing.Vertex.Bytes == InProgram.Vertex.Bytes &&
		    Existing.Pixel.Bytes == InProgram.Pixel.Bytes)
		{
			Entry.Ownership.Add(InOwners);
			++Impl->Stats.PipelineReuses;
			return Entry.Resource;
		}
	}
	return GetPipeline(DescribeMaterialPipeline(InProgram, InPass, InLayout, InAttributes, InStride, InTopology,
	                                            InTarget, bInMirrored),
	                   InOwners);
}

FPipeline FMaterialGpuCache::GetPipeline(const FPipelineDesc& InDescription, const FMaterialResourceOwners& InOwners)
{
	Impl->CheckOwner();
	if (!InDescription.Layout)
	{
		throw std::invalid_argument("Material pipeline requires a generic resource layout");
	}
	const auto Hash = std::hash<std::string>{}(InDescription.Vertex.CacheKey + "/" + InDescription.Pixel.CacheKey);
	const auto [Begin, End] = Impl->Pipelines.equal_range(Hash);
	for (auto Iterator = Begin; Iterator != End; ++Iterator)
	{
		auto& Entry = Iterator->second;
		if (SamePipeline(Entry.Description, InDescription))
		{
			Entry.Ownership.Add(InOwners);
			++Impl->Stats.PipelineReuses;
			return Entry.Resource;
		}
	}
	FImpl::FPipelineEntry Entry;
	Entry.Description = InDescription;
	Entry.Ownership.Add(InOwners);
	Entry.Resource = Impl->Device.CreatePipeline(InDescription);
	const auto Iterator = Impl->Pipelines.emplace(Hash, std::move(Entry));
	++Impl->Stats.PipelinesCreated;
	return Iterator->second.Resource;
}
} // namespace Hyperion
