#pragma once

namespace Hyperion
{
struct FViewport
{
	float X{};
	float Y{};
	float Width{};
	float Height{};
	float MinDepth{};
	float MaxDepth = 1;
	bool operator==(const FViewport&) const = default;
};
} // namespace Hyperion
