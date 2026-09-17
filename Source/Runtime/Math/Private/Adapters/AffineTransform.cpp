#include "Hyperion/Math/AffineTransform.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <numbers>
#include <stdexcept>

namespace Hyperion
{
namespace
{
FVec3 Owned(const glm::dvec3& InValue)
{
	return {float(InValue.x), float(InValue.y), float(InValue.z)};
}

glm::dvec3 Perpendicular(const glm::dvec3& InAxis)
{
	const auto Absolute = glm::abs(InAxis);
	const glm::dvec3 Basis = Absolute.y <= Absolute.x && Absolute.y <= Absolute.z ? glm::dvec3(0, 1, 0)
	                         : Absolute.z <= Absolute.x                           ? glm::dvec3(0, 0, 1)
	                                                                              : glm::dvec3(1, 0, 0);
	return glm::normalize(Basis - InAxis * glm::dot(Basis, InAxis));
}

glm::dmat3 Basis(const glm::dmat3& InMatrix)
{
	auto X = InMatrix[0];
	if (glm::length(X) == 0)
	{
		X = glm::cross(InMatrix[1], InMatrix[2]);
		if (glm::length(X) == 0)
		{
			const auto Other = glm::length(InMatrix[1]) > 0 ? InMatrix[1] : InMatrix[2];
			X = glm::length(Other) > 0 ? Perpendicular(glm::normalize(Other)) : glm::dvec3(1, 0, 0);
		}
	}
	X = glm::normalize(X);
	auto Y = InMatrix[1] - X * glm::dot(X, InMatrix[1]);
	if (glm::length(Y) <= glm::length(InMatrix[1]) * 1e-12)
	{
		Y = glm::cross(InMatrix[2], X);
		if (glm::length(Y) <= glm::length(InMatrix[2]) * 1e-12)
		{
			Y = Perpendicular(X);
		}
	}
	Y = glm::normalize(Y);
	auto Z = glm::normalize(glm::cross(X, Y));
	Y = glm::cross(Z, X);
	if (glm::determinant(InMatrix) < 0)
	{
		X = -X;
		Z = -Z;
	}
	return {X, Y, Z};
}

FVec3 EulerAngles(const glm::dmat3& InRotation)
{
	const double CosY = std::hypot(InRotation[0][0], InRotation[0][1]);
	const double Y = std::atan2(-InRotation[0][2], CosY);
	const double X =
	    CosY > 1e-7 ? std::atan2(InRotation[1][2], InRotation[2][2]) : std::atan2(-InRotation[2][1], InRotation[1][1]);
	const double Z = CosY > 1e-7 ? std::atan2(InRotation[0][1], InRotation[0][0]) : 0;
	return {float(X == 0 ? 0 : X), float(Y == 0 ? 0 : Y), float(Z == 0 ? 0 : Z)};
}

glm::dmat3 RotationMatrix(FVec3 InAngles)
{
	const double X = std::remainder(double(InAngles.X), 2 * std::numbers::pi);
	const double Y = std::remainder(double(InAngles.Y), 2 * std::numbers::pi);
	const double Z = std::remainder(double(InAngles.Z), 2 * std::numbers::pi);
	const double Cx = std::cos(X);
	const double Sx = std::sin(X);
	const double Cy = std::cos(Y);
	const double Sy = std::sin(Y);
	const double Cz = std::cos(Z);
	const double Sz = std::sin(Z);
	return {{Cy * Cz, Cy * Sz, -Sy},
	        {Cz * Sx * Sy - Cx * Sz, Cx * Cz + Sx * Sy * Sz, Cy * Sx},
	        {Sx * Sz + Cx * Cz * Sy, Cx * Sy * Sz - Cz * Sx, Cx * Cy}};
}
} // namespace

FAffineTransform DecomposeAffine(const FMat4& InMatrix)
{
	const auto& Values = InMatrix.Values;
	if (!std::all_of(Values.begin(), Values.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }) ||
	    Values[3] != 0 || Values[7] != 0 || Values[11] != 0 || Values[15] != 1)
	{
		throw std::invalid_argument("Display transform must be finite and affine");
	}
	const glm::dmat3 Linear{
	    {Values[0], Values[1], Values[2]}, {Values[4], Values[5], Values[6]}, {Values[8], Values[9], Values[10]}};
	const auto Rotation = Basis(Linear);
	const auto Stretch = glm::transpose(Rotation) * Linear;
	return {{Values[12], Values[13], Values[14]},
	        EulerAngles(Rotation),
	        Owned({Stretch[0][0], Stretch[1][1], Stretch[2][2]}),
	        Owned({Stretch[1][0], Stretch[2][0], Stretch[2][1]})};
}

FMat4 ComposeAffine(const FAffineTransform& InTransform)
{
	const glm::dmat3 Stretch{{InTransform.Scale.X, 0, 0},
	                         {InTransform.Shear.X, InTransform.Scale.Y, 0},
	                         {InTransform.Shear.Y, InTransform.Shear.Z, InTransform.Scale.Z}};
	const auto Linear = RotationMatrix(InTransform.Rotation) * Stretch;
	auto Result = Identity();
	for (unsigned Column = 0; Column < 3; ++Column)
	{
		for (unsigned Row = 0; Row < 3; ++Row)
		{
			Result.Values[Column * 4 + Row] = float(Linear[Column][Row]);
		}
	}
	Result.Values[12] = InTransform.Position.X;
	Result.Values[13] = InTransform.Position.Y;
	Result.Values[14] = InTransform.Position.Z;
	if (!std::all_of(Result.Values.begin(), Result.Values.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }))
	{
		throw std::invalid_argument("Composed transform must be finite");
	}
	return Result;
}
} // namespace Hyperion
