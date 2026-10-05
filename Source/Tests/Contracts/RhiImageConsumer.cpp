#include "Hyperion/RHI/RHIDevice.h"
#include <type_traits>
#include <utility>

static_assert(std::is_same_v<
              decltype(std::declval<Hyperion::IRHIDevice&>().CreateTexture(std::declval<const Hyperion::FImage&>())),
              Hyperion::FTexture>);
static_assert(
    std::is_same_v<decltype(std::declval<Hyperion::IRHISwapchain&>().EndFrame({}, false, true)), Hyperion::FImage>);

int main()
{
	const Hyperion::FImage Image{1, 1, Hyperion::EColorSpace::Linear, {-.5f, 2.f, 3.5f, 1.f}};
	return Image.Rgba[2] == 3.5f ? 0 : 1;
}
