#include "RenderGraphResources.h"
#include <stdexcept>

namespace Hyperion
{
FGraphDepthResource& FGraphDepthResources::Find(const FTexture& InTexture)
{
	if (!InTexture)
	{
		throw std::invalid_argument("Empty graph depth resource");
	}
	auto& Resource = Textures[InTexture.Payload.get()];
	Resource.Texture = InTexture;
	return Resource;
}

FGraphDepthResources::FGraphDepthResources(std::span<const FTexture> InImports)
{
	for (const auto& Texture : InImports)
	{
		Find(Texture).bInitialized = true;
	}
}

void FGraphDepthResources::Transition(FGraphDepthResource& InResource, EResourceState InState,
                                      FPassCommands& InCommands)
{
	if (InResource.State != InState)
	{
		InCommands.TextureTransitions.push_back({InResource.Texture, InResource.State, InState});
		InResource.State = InState;
	}
}

void FGraphDepthResources::Compile(FPassCommands& InCommands)
{
	if (InCommands.DepthTarget)
	{
		if (!InCommands.bUseDepth || InCommands.bUseStencil || InCommands.DepthFormat != ERHIDepthFormat::D32)
		{
			throw std::invalid_argument("Explicit depth target requires D32 depth without stencil");
		}
		auto& Resource = Find(InCommands.DepthTarget);
		if (!Resource.bInitialized && (!InCommands.bClearDepth || InCommands.Viewport))
		{
			throw std::invalid_argument("Graph must initialize the whole depth texture before loading or sampling");
		}
		Resource.bInitialized |= InCommands.bClearDepth && !InCommands.Viewport;
		Transition(Resource, EResourceState::DepthWrite, InCommands);
	}
	for (const auto& Texture : InCommands.SampledDepth)
	{
		if (Texture == InCommands.DepthTarget)
		{
			throw std::invalid_argument("Graph cannot sample its writable depth attachment");
		}
		auto& Resource = Find(Texture);
		if (!Resource.bInitialized)
		{
			throw std::invalid_argument("Graph samples undefined depth contents");
		}
		Transition(Resource, EResourceState::ShaderRead, InCommands);
	}
}

void FGraphDepthResources::Finish(FPassCommands& InCommands)
{
	for (auto& [Identity, Resource] : Textures)
	{
		Transition(Resource, EResourceState::ShaderRead, InCommands);
	}
}
} // namespace Hyperion
