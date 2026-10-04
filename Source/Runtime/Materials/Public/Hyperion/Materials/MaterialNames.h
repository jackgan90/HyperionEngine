#pragma once

namespace Hyperion
{
// Shared spellings; material usage and variant extension points remain open strings.
namespace MaterialUsages
{
inline constexpr char Forward[] = "Forward";
inline constexpr char HdrForwardOpaque[] = "HdrForwardOpaque";
inline constexpr char HdrTransparent[] = "HdrTransparent";
inline constexpr char HdrCompatibility[] = "HdrCompatibility";
inline constexpr char DeferredBase[] = "DeferredBase";
inline constexpr char ShadowDepth[] = "ShadowDepth";
inline constexpr char SilhouetteMask[] = "SilhouetteMask";
} // namespace MaterialUsages

namespace MaterialVariants
{
inline constexpr char Default[] = "Default";
inline constexpr char Instance[] = "Instance";
} // namespace MaterialVariants
} // namespace Hyperion
