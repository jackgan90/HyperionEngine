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
FVec3 Add(FVec3 InA, FVec3 InB);
FVec3 Subtract(FVec3 InA, FVec3 InB);
FVec3 ScaleVector(FVec3 InValue, float InScale);
float Dot(FVec3 InA, FVec3 InB);
FVec3 Cross(FVec3 InA, FVec3 InB);
float Length(FVec3 InValue);
FVec3 Normalize(FVec3 InValue);
FMat4 ComposeTRS(FVec3 InTranslation, FVec4 InRotation, FVec3 InScale);
FMat4 LookAt(FVec3 InEye, FVec3 InTarget, FVec3 InUp = {0, 1, 0});
FMat4 Perspective(float InVerticalRadians, float InAspect, float InNear, float InFar);
FMat4 Inverse(const FMat4& InMatrix);
FMat4 NormalMatrix(const FMat4& InMatrix);
float Determinant(const FMat4& InMatrix);
} // namespace Hyperion
