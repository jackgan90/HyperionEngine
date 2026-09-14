#include "Hyperion/Renderer/ComputePass.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "RenderResourcesInternal.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
namespace
{
FGraphTextureImport DescribeTexture(const FComputeTextureParameter& InParameter, std::uint32_t InMip)
{
	if (!InParameter.Source || !InParameter.MipCount || InParameter.FirstMip >= 32 ||
	    InParameter.MipCount > 32 - InParameter.FirstMip ||
	    (InParameter.Access != EResourceState::ShaderRead && InParameter.Access != EResourceState::ShaderWrite))
	{
		throw std::invalid_argument("Invalid compute texture source, mip range or access");
	}
	const auto& Source = *InParameter.Source;
	FGraphTextureImport Import;
	Import.Name = "Compute texture " + std::to_string(Source.GetIdentity());
	Import.Target.Kind = ERenderTargetKind::Texture;
	Import.Identity = InParameter.Source;
	Import.MipLevel = InMip;
	Import.bInitialized = InParameter.bInitialized;
	std::uint32_t MipCount = 1;
	if (const auto* Storage = Source.GetStorage())
	{
		Import.Size = {Storage->Width, Storage->Height};
		Import.ColorFormat = GetRenderColorFormat(Storage->Format);
		Import.bStorage = true;
		MipCount = Storage->MipCount;
	}
	else if (const auto* Depth = Source.GetDepthTarget())
	{
		Import.Size = {Depth->Width, Depth->Height};
		Import.DepthFormat = ERHIDepthFormat::D32;
	}
	else if (const auto* Color = Source.GetColorTarget())
	{
		Import.Size = {Color->Width, Color->Height};
		Import.ColorFormat = GetRenderColorFormat(Color->Format);
	}
	else
	{
		const auto& Mips = Source.GetMips();
		if (Mips.empty())
		{
			throw std::invalid_argument("Compute sampled source has no mips");
		}
		Import.Size = {Mips.front().Width, Mips.front().Height};
		Import.ColorFormat = Source.GetFormat() == ETextureFormat::Rgba16Float
		                         ? ERHIColorFormat::Rgba16Float
		                         : (Source.GetFormat() == ETextureFormat::Rgba32Float ? ERHIColorFormat::Rgba32Float
		                                                                              : ERHIColorFormat::Rgba8Unorm);
		Import.bSampledOnly = true;
		Import.bInitialized = true;
		MipCount = static_cast<std::uint32_t>(Mips.size());
	}
	if (InMip >= MipCount || InParameter.FirstMip + InParameter.MipCount > MipCount ||
	    (InParameter.Access == EResourceState::ShaderWrite && (!Import.bStorage || InParameter.MipCount != 1)) ||
	    (InParameter.Access == EResourceState::ShaderRead && InParameter.bFullOverwrite))
	{
		throw std::invalid_argument("Compute texture view exceeds source mips or usage");
	}
	Import.Size = {std::max(1U, Import.Size.Width >> InMip), std::max(1U, Import.Size.Height >> InMip)};
	return Import;
}
} // namespace

FBuffer FRenderResourcePreparation::ResolveBuffer(std::shared_ptr<const FMaterialReadBufferSource> InSource,
                                                  std::shared_ptr<const void> InLifetime) const
{
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::lock_guard Lock(Owner.Mutex);
	if (Owner.bClosed || !InLifetime)
	{
		throw std::logic_error("Compute buffer resolution requires live scope");
	}
	Owner.EnsureMaterialCaches();
	Owner.TrackScope(InLifetime);
	return Owner.MaterialGpu->GetBuffer(std::move(InSource), {InLifetime});
}

FComputePass FRenderResourcePreparation::DeclareCompute(FRenderGraph& InGraph, const FComputePassDesc& InPass) const
{
	Coordinator->Tasks.Require({EDomain::Render});
	if (!InPass.Lifetime || InPass.Shader.Source.empty() || InPass.Shader.Entry.empty() || !InPass.Extent[0] ||
	    !InPass.Extent[1] || !InPass.Extent[2])
	{
		throw std::invalid_argument("Compute pass requires a shader, live scope and nonempty extent");
	}
	FComputePass Pass;
	Pass.Name = InPass.Name;
	Pass.After = InPass.After;
	for (const auto& Parameter : InPass.Textures)
	{
		if (!Parameter.MipCount || Parameter.FirstMip >= 32 || Parameter.MipCount > 32 - Parameter.FirstMip)
		{
			throw std::invalid_argument("Invalid compute mip range");
		}
		for (std::uint32_t Mip = Parameter.FirstMip; Mip < Parameter.FirstMip + Parameter.MipCount; ++Mip)
		{
			auto Import = DescribeTexture(Parameter, Mip);
			Import.Resolve = [Preparation = *this, Source = Parameter.Source, Lifetime = InPass.Lifetime]
			{
				return Preparation.ResolveTexture(Source, Lifetime);
			};
			const auto Texture = InGraph.Import(std::move(Import));
			if (Parameter.Access == EResourceState::ShaderWrite)
			{
				Pass.Writes.push_back({Texture, Parameter.bFullOverwrite});
			}
			else if (std::find(Pass.Reads.begin(), Pass.Reads.end(), Texture) == Pass.Reads.end())
			{
				Pass.Reads.push_back(Texture);
			}
			InGraph.Export(Texture, EResourceState::ShaderRead);
		}
	}
	for (const auto& Parameter : InPass.Buffers)
	{
		Parameter.View.Validate();
		const auto Source = Parameter.View.Source;
		if (Parameter.bFullOverwrite && (Parameter.View.Offset || Parameter.View.Size != Source->GetSize()))
		{
			throw std::invalid_argument("Full buffer overwrite requires full view coverage");
		}
		FGraphBufferImport Import{Parameter.Name,
		                          {},
		                          Source->GetSize(),
		                          BufferUsage(ERHIBufferUsage::StructuredRead) | BufferUsage(ERHIBufferUsage::RawRead) |
		                              (Source->IsStorage() ? BufferUsage(ERHIBufferUsage::StructuredWrite) |
		                                                         BufferUsage(ERHIBufferUsage::RawWrite)
		                                                   : 0U),
		                          Source->IsStorage() ? Parameter.bInitialized : true,
		                          EResourceState::ShaderRead,
		                          Source,
		                          [Preparation = *this, Source, Lifetime = InPass.Lifetime]
		                          {
			                          return Preparation.ResolveBuffer(Source, Lifetime);
		                          }};
		const auto Buffer = InGraph.Import(std::move(Import));
		const auto Existing = std::find_if(Pass.Buffers.begin(), Pass.Buffers.end(),
		                                   [&](const auto& InAccess)
		                                   {
			                                   return InAccess.Buffer == Buffer;
		                                   });
		if (Existing == Pass.Buffers.end())
		{
			Pass.Buffers.push_back({Buffer, Parameter.Access, Parameter.bFullOverwrite});
		}
		else if (Existing->State != Parameter.Access || Existing->bFullOverwrite != Parameter.bFullOverwrite)
		{
			throw std::invalid_argument("Conflicting compute buffer access declarations");
		}
		InGraph.Export(Buffer, EResourceState::ShaderRead);
	}
	return Pass;
}

void FRenderSession::AppendCompute(FRenderGraph& InGraph, FComputePassDesc InPass, bool bInDeferPreparation)
{
	Tasks.Require({EDomain::Render});
	const auto Preparation = Resources.GetPreparation();
	auto Pass = Preparation.DeclareCompute(InGraph, InPass);
	Pass.Prepare = [Preparation, Description = std::move(InPass)]
	{
		return std::vector<FDispatchPacket>{Preparation.BuildCompute(Description)};
	};
	if (!bInDeferPreparation)
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Pass.Dispatches = Pass.Prepare();
		                          }));
		Pass.Prepare = {};
	}
	InGraph.AddCompute(std::move(Pass));
}

void AddComputePass(FRenderSession& InSession, FRenderGraph& InGraph, FComputePassDesc InPass, bool bInDeferPreparation)
{
	InSession.AppendCompute(InGraph, std::move(InPass), bInDeferPreparation);
}
} // namespace Hyperion
