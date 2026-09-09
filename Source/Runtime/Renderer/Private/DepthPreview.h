#pragma once
#include "Hyperion/Renderer/RenderResources.h"

namespace Hyperion
{
struct FDepthPreview
{
	FResourceBindingLayout Layout;
	FDrawPacket Draw;
	FTexture Texture;
	std::weak_ptr<const void> Lifetime;
	void Collect();
};
} // namespace Hyperion
