#pragma once
#include <array>

namespace Hyperion
{
struct FVec2
{
	float X{};
	float Y{};
};

struct FVec3
{
	float X{};
	float Y{};
	float Z{};
};

struct FVec4
{
	float X{};
	float Y{};
	float Z{};
	float W{};
};

// Column-major matrices multiplied by column vectors. Angles use radians.
struct FMat4
{
	std::array<float, 16> Values{};
};

FMat4 Identity();
FMat4 Scale(FVec3 InValue);
FMat4 Translation(FVec3 InValue);
FMat4 Multiply(const FMat4& InA, const FMat4& InB);
FVec4 Transform(const FMat4& InMatrix, FVec4 InValue);
} // namespace Hyperion
