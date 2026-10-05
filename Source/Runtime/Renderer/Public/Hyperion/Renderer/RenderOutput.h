#pragma once
#include "Hyperion/ImageData/ImageData.h"
#include "Hyperion/Platform/Window.h"
#include "Hyperion/RenderControls/RenderOutput.h"

namespace Hyperion
{
std::shared_ptr<FPendingImageOutput> PrepareImageOutput(const FImageOutputRequest& InRequest);
void CompleteImageOutput(FPendingImageOutput& InPending, const FImage& InImage, std::uint64_t InFrame);
void DescribeImageOutput(FPendingImageOutput& InPending, FSize InSize, std::uint64_t InFrame);
} // namespace Hyperion
