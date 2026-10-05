#pragma once
#include <cstdint>

#include <span>
#include <string_view>

namespace Hyperion
{
enum class ESceneCullingMode
{
	None,
	Linear,
	Bvh
};

enum class EOutlineOverlapMode : std::uint8_t
{
	Union,
	PerObject
};

struct FSceneCullingOption
{
	ESceneCullingMode Id;
	std::uint32_t WireValue;
	std::string_view Label;
};

struct FOutlineOverlapOption
{
	EOutlineOverlapMode Id;
	std::uint32_t WireValue;
	std::string_view Label;
};

std::span<const FSceneCullingOption> SceneCullingOptions();
std::span<const FOutlineOverlapOption> OutlineOverlapOptions();
bool IsSceneCullingWireValue(std::uint32_t InValue);
bool IsOutlineOverlapWireValue(std::uint32_t InValue);
ESceneCullingMode ParseSceneCullingMode(std::uint32_t InValue);
EOutlineOverlapMode ParseOutlineOverlapMode(std::uint32_t InValue);
std::uint32_t ToCullingWireValue(ESceneCullingMode InId);
std::uint32_t ToOutlineWireValue(EOutlineOverlapMode InId);

// Order and labels are presentation choices; supported identities and wire meanings are fixed.
void ValidateViewportOptionPresentation(std::span<const FSceneCullingOption> InOptions);
void ValidateViewportOptionPresentation(std::span<const FOutlineOverlapOption> InOptions);
} // namespace Hyperion
