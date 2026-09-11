#include "Hyperion/Math/Math.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>

namespace Hyperion
{
namespace
{
FMat4 Owned(const glm::mat4& InV)
{
	FMat4 R;
	std::copy_n(glm::value_ptr(InV), 16, R.Values.begin());
	return R;
}

glm::mat4 Native(const FMat4& InV)
{
	return glm::make_mat4(InV.Values.data());
}
} // namespace

FVec3 Add(FVec3 InA, FVec3 InB)
{
	return {InA.X + InB.X, InA.Y + InB.Y, InA.Z + InB.Z};
}

FVec3 Subtract(FVec3 InA, FVec3 InB)
{
	return {InA.X - InB.X, InA.Y - InB.Y, InA.Z - InB.Z};
}

FVec3 ScaleVector(FVec3 InValue, float InScale)
{
	return {InValue.X * InScale, InValue.Y * InScale, InValue.Z * InScale};
}

float Dot(FVec3 InA, FVec3 InB)
{
	return InA.X * InB.X + InA.Y * InB.Y + InA.Z * InB.Z;
}

FVec3 Cross(FVec3 InA, FVec3 InB)
{
	return {InA.Y * InB.Z - InA.Z * InB.Y, InA.Z * InB.X - InA.X * InB.Z, InA.X * InB.Y - InA.Y * InB.X};
}

float Length(FVec3 InValue)
{
	return std::sqrt(Dot(InValue, InValue));
}

FVec3 Normalize(FVec3 InValue)
{
	const auto Magnitude = Length(InValue);
	return Magnitude > 1e-20f ? ScaleVector(InValue, 1 / Magnitude) : FVec3{0, 1, 0};
}

FMat4 ComposeTRS(FVec3 InTranslation, FVec4 InRotation, FVec3 InScale)
{
	const glm::quat Rotation(InRotation.W, InRotation.X, InRotation.Y, InRotation.Z);
	if (glm::length(Rotation) < 1e-8f)
	{
		throw std::runtime_error("Invalid rotation quaternion");
	}
	return Multiply(Multiply(Translation(InTranslation), Owned(glm::mat4_cast(glm::normalize(Rotation)))),
	                Scale(InScale));
}

FMat4 LookAt(FVec3 InEye, FVec3 InTarget, FVec3 InUp)
{
	return Owned(glm::lookAtRH(glm::vec3(InEye.X, InEye.Y, InEye.Z), glm::vec3(InTarget.X, InTarget.Y, InTarget.Z),
	                           glm::vec3(InUp.X, InUp.Y, InUp.Z)));
}

FMat4 Perspective(float InVerticalRadians, float InAspect, float InNear, float InFar, EDepthConvention InConvention)
{
	if (!(InAspect > 0 && InNear > 0 && InFar > InNear && InVerticalRadians > 0 && InVerticalRadians < 3.14f) ||
	    !std::isfinite(InAspect) || !std::isfinite(InNear) || !std::isfinite(InFar) ||
	    InConvention > EDepthConvention::Reversed)
	{
		throw std::invalid_argument("Invalid camera projection");
	}
	// Construct reversed coefficients directly rather than subtracting nearly equal clip Z/W values.
	return InConvention == EDepthConvention::Reversed
	           ? Owned(glm::perspectiveRH_ZO(InVerticalRadians, InAspect, InFar, InNear))
	           : Owned(glm::perspectiveRH_ZO(InVerticalRadians, InAspect, InNear, InFar));
}

FMat4 ClipDepthTransform(EDepthConvention InConvention)
{
	if (InConvention > EDepthConvention::Reversed)
	{
		throw std::invalid_argument("Invalid depth convention");
	}
	auto Result = Identity();
	Result.Values[10] = GetDepthDirection(InConvention);
	Result.Values[14] = 1.f - GetDepthClearValue(InConvention);
	return Result;
}

float Determinant(const FMat4& InMatrix)
{
	return glm::determinant(Native(InMatrix));
}

FMat4 Inverse(const FMat4& InMatrix)
{
	const float Det = Determinant(InMatrix);
	if (!std::isfinite(Det) || std::abs(Det) < 1e-20f)
	{
		throw std::invalid_argument("Cannot invert a singular or nonfinite matrix");
	}
	auto Result = Owned(glm::inverse(Native(InMatrix)));
	if (!std::all_of(Result.Values.begin(), Result.Values.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }))
	{
		throw std::invalid_argument("Matrix inverse is nonfinite");
	}
	return Result;
}

FMat4 NormalMatrix(const FMat4& InMatrix)
{
	if (std::abs(Determinant(InMatrix)) < 1e-20f)
	{
		return Identity();
	}
	return Owned(glm::transpose(glm::inverse(Native(InMatrix))));
}

FMat4 Identity()
{
	return Owned(glm::mat4(1));
}

FMat4 Scale(FVec3 InV)
{
	return Owned(glm::scale(glm::mat4(1), glm::vec3(InV.X, InV.Y, InV.Z)));
}

FMat4 Translation(FVec3 InV)
{
	return Owned(glm::translate(glm::mat4(1), glm::vec3(InV.X, InV.Y, InV.Z)));
}

FMat4 Multiply(const FMat4& InA, const FMat4& InB)
{
	return Owned(Native(InA) * Native(InB));
}

FVec4 Transform(const FMat4& InM, FVec4 InV)
{
	auto R = Native(InM) * glm::vec4(InV.X, InV.Y, InV.Z, InV.W);
	return {R.x, R.y, R.z, R.w};
}
} // namespace Hyperion
