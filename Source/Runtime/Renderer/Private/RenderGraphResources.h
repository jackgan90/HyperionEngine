#pragma once
#include "Hyperion/Renderer/RenderGraph.h"
#include <map>

namespace Hyperion
{
struct FGraphDepthResource
{
	FTexture Texture;
	EResourceState State = EResourceState::ShaderRead;
	bool bInitialized{};
};

class FGraphDepthResources
{
public:
	explicit FGraphDepthResources(std::span<const FTexture> InImports);
	void Compile(FPassCommands& InCommands);
	void Finish(FPassCommands& InCommands);

private:
	std::map<const IRHITexture*, FGraphDepthResource> Textures;
	FGraphDepthResource& Find(const FTexture& InTexture);
	void Transition(FGraphDepthResource& InResource, EResourceState InState, FPassCommands& InCommands);
};
} // namespace Hyperion
