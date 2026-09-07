#include "MaterialGpuCacheInternal.h"

namespace Hyperion
{
namespace
{
ERHIAddressMode Address(EMaterialAddressMode InAddress)
{
	switch (InAddress)
	{
		case EMaterialAddressMode::Repeat:
			return ERHIAddressMode::Repeat;
		case EMaterialAddressMode::Clamp:
			return ERHIAddressMode::Clamp;
		case EMaterialAddressMode::Mirror:
			return ERHIAddressMode::Mirror;
		case EMaterialAddressMode::Border:
			return ERHIAddressMode::Border;
		case EMaterialAddressMode::MirrorOnce:
			return ERHIAddressMode::MirrorOnce;
	}
	throw std::invalid_argument("Invalid material sampler addressing");
}

FSamplerDesc DescribeSampler(const FMaterialSampler& InSampler)
{
	InSampler.Validate();
	FSamplerDesc Result;
	Result.U = Address(InSampler.U);
	Result.V = Address(InSampler.V);
	Result.W = Address(InSampler.W);
	Result.bMinLinear = InSampler.bMinLinear;
	Result.bMagLinear = InSampler.bMagLinear;
	Result.bMipLinear = InSampler.bMipLinear;
	Result.bComparison = InSampler.bComparison;
	Result.MipLodBias = InSampler.MipLodBias;
	Result.MinLod = InSampler.MinLod;
	Result.MaxLod = InSampler.MaxLod;
	Result.MaxAnisotropy = InSampler.MaxAnisotropy;
	Result.BorderColor = InSampler.BorderColor;
	return Result;
}
} // namespace

FTexture FMaterialGpuCache::FImpl::Texture(std::shared_ptr<const FMaterialTextureSource> InSource,
                                           const FMaterialResourceOwners& InOwners)
{
	if (!InSource)
	{
		throw std::invalid_argument("Missing material texture source");
	}
	auto [Iterator, bInserted] = Textures.try_emplace(InSource->GetIdentity());
	auto& Entry = Iterator->second;
	Entry.Ownership.Add(InOwners);
	if (!Entry.Resource)
	{
		Entry.Description = InSource;
		FTextureDesc Description;
		Description.bSrgb = InSource->GetEncoding() == EMaterialTextureEncoding::Srgb;
		for (const auto& Mip : InSource->GetMips())
		{
			Description.Mips.push_back({Mip.Width, Mip.Height, Mip.Bytes});
		}
		const std::array Descriptions{std::move(Description)};
		auto Native = Device.CreateTexturesAsync(Descriptions);
		if (Native.size() != 1 || !Native.front())
		{
			throw std::runtime_error("Backend returned an incomplete texture upload");
		}
		Entry.Resource = std::move(Native.front());
		++Stats.TextureUploads;
	}
	return Entry.Resource;
}

FBuffer FMaterialGpuCache::FImpl::Buffer(std::shared_ptr<const FMaterialReadBufferSource> InSource,
                                         const FMaterialResourceOwners& InOwners)
{
	if (!InSource)
	{
		throw std::invalid_argument("Missing material read buffer source");
	}
	auto [Iterator, bInserted] = Buffers.try_emplace(InSource->GetIdentity());
	auto& Entry = Iterator->second;
	Entry.Ownership.Add(InOwners);
	if (!Entry.Resource)
	{
		Entry.Description = InSource;
		Entry.Resource =
		    Device.CreateBuffer({InSource->GetBytes().size(),
		                         BufferUsage(ERHIBufferUsage::StructuredRead) | BufferUsage(ERHIBufferUsage::RawRead)},
		                        InSource->GetBytes());
		++Stats.ReadBufferUploads;
	}
	return Entry.Resource;
}

FSampler FMaterialGpuCache::FImpl::Sampler(const FMaterialSampler& InSampler, const FMaterialResourceOwners& InOwners)
{
	for (auto& Entry : Samplers)
	{
		if (Entry.Description == InSampler)
		{
			Entry.Ownership.Add(InOwners);
			return Entry.Resource;
		}
	}
	FSamplerEntry Entry;
	Entry.Description = InSampler;
	Entry.Ownership.Add(InOwners);
	Entry.Resource = Device.CreateSampler(DescribeSampler(InSampler));
	Samplers.push_back(std::move(Entry));
	++Stats.SamplersCreated;
	return Samplers.back().Resource;
}

FResourceBindingValue FMaterialGpuCache::FImpl::Resource(const FMaterialValue& InValue,
                                                         const FMaterialResourceOwners& InOwners)
{
	InValue.Validate();
	switch (InValue.Type.Kind)
	{
		case EMaterialValueKind::Texture2D:
			return Texture(InValue.Texture, InOwners);
		case EMaterialValueKind::Sampler:
			return Sampler(InValue.Sampler, InOwners);
		case EMaterialValueKind::ReadBuffer:
		{
			const auto& View = InValue.Buffer;
			return FReadBufferView{Buffer(View.Source, InOwners),
			                       View.Kind == EMaterialBufferViewKind::Structured ? ERHIBufferViewKind::Structured
			                                                                        : ERHIBufferViewKind::Raw,
			                       View.Offset, View.Size, View.Stride};
		}
		default:
			throw std::invalid_argument("Material resource parameter has a non-resource value");
	}
}
} // namespace Hyperion
