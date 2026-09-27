#pragma once
#include "Hyperion/Platform/Window.h"

namespace Hyperion
{
std::string ReadTypedClipboard(FNativeSurface InSurface, const std::string& InFormat);
void WriteTypedClipboard(FNativeSurface InSurface, const std::string& InFormat, const std::string& InData,
                         const std::string& InText);
} // namespace Hyperion
