#include "Hyperion/Environment/SkyAsset.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Hyperion
{
FVec3 CubeDirection(unsigned InFace, float InU, float InV)
{
	switch (InFace)
	{
		case 0:
			return Normalize({1, -InV, -InU});
		case 1:
			return Normalize({-1, -InV, InU});
		case 2:
			return Normalize({InU, 1, InV});
		case 3:
			return Normalize({InU, -1, -InV});
		case 4:
			return Normalize({InU, -InV, 1});
		case 5:
			return Normalize({-InU, -InV, -1});
	}
	throw std::invalid_argument("Invalid cube face");
}

namespace
{
struct FCubeCoordinate
{
	unsigned Face{};
	float U{};
	float V{};
};

FCubeCoordinate Coordinate(FVec3 InDirection)
{
	const float X = std::abs(InDirection.X);
	const float Y = std::abs(InDirection.Y);
	const float Z = std::abs(InDirection.Z);
	if (X >= Y && X >= Z && X > 0)
	{
		return InDirection.X >= 0 ? FCubeCoordinate{0, -InDirection.Z / X, -InDirection.Y / X}
		                          : FCubeCoordinate{1, InDirection.Z / X, -InDirection.Y / X};
	}
	if (Y >= Z && Y > 0)
	{
		return InDirection.Y >= 0 ? FCubeCoordinate{2, InDirection.X / Y, InDirection.Z / Y}
		                          : FCubeCoordinate{3, InDirection.X / Y, -InDirection.Z / Y};
	}
	if (Z > 0)
	{
		return InDirection.Z >= 0 ? FCubeCoordinate{4, InDirection.X / Z, -InDirection.Y / Z}
		                          : FCubeCoordinate{5, -InDirection.X / Z, -InDirection.Y / Z};
	}
	throw std::invalid_argument("Environment direction must be nonzero");
}

FVec3 Texel(const FTextureAsset& InCube, std::size_t InMip, unsigned InFace, int InX, int InY)
{
	const auto& Mip = InCube.Mips[InMip];
	const int Size = static_cast<int>(Mip.Width);
	if (InX < 0 || InY < 0 || InX >= Size || InY >= Size)
	{
		const auto Adjacent = Coordinate(CubeDirection(InFace, (InX + .5f) * 2 / Size - 1, (InY + .5f) * 2 / Size - 1));
		InFace = Adjacent.Face;
		InX = std::clamp(int((Adjacent.U * .5f + .5f) * Size), 0, Size - 1);
		InY = std::clamp(int((Adjacent.V * .5f + .5f) * Size), 0, Size - 1);
	}
	const auto Value = ReadTexturePixel(Mip, InCube.Format, (std::size_t(InFace) * Size + InY) * Size + InX);
	return {Value[0], Value[1], Value[2]};
}

FVec3 SampleMip(const FTextureAsset& InCube, FCubeCoordinate InCoordinate, std::size_t InMip)
{
	const auto Size = InCube.Mips[InMip].Width;
	const float X = (InCoordinate.U * .5f + .5f) * Size - .5f;
	const float Y = (InCoordinate.V * .5f + .5f) * Size - .5f;
	const int Left = int(std::floor(X));
	const int Top = int(std::floor(Y));
	FVec3 Result;
	for (int Row = 0; Row < 2; ++Row)
	{
		for (int Column = 0; Column < 2; ++Column)
		{
			const float Weight = (Column ? X - Left : 1 - X + Left) * (Row ? Y - Top : 1 - Y + Top);
			Result =
			    Add(Result, ScaleVector(Texel(InCube, InMip, InCoordinate.Face, Left + Column, Top + Row), Weight));
		}
	}
	return Result;
}

double Area(double InX, double InY)
{
	return std::atan2(InX * InY, std::sqrt(InX * InX + InY * InY + 1));
}
} // namespace

FVec3 SampleEnvironment(const FTextureAsset& InCube, FVec3 InDirection, float InMip)
{
	if (InCube.Dimension != ETextureDimension::Cube || InCube.Mips.empty() || !std::isfinite(InMip))
	{
		throw std::invalid_argument("Environment sampling requires a cube texture and finite mip");
	}
	const auto Position = Coordinate(InDirection);
	const float Level = std::clamp(InMip, 0.f, float(InCube.Mips.size() - 1));
	const auto Low = std::size_t(Level);
	const auto High = std::min(Low + 1, InCube.Mips.size() - 1);
	const auto A = SampleMip(InCube, Position, Low);
	if (Level == Low)
	{
		return A;
	}
	return Add(ScaleVector(A, 1 - (Level - Low)), ScaleVector(SampleMip(InCube, Position, High), Level - Low));
}

std::array<float, 9> EnvironmentShBasis(FVec3 InDirection)
{
	const auto N = Normalize(InDirection);
	return {.282094792f,
	        .488602512f * N.Y,
	        .488602512f * N.Z,
	        .488602512f * N.X,
	        1.092548431f * N.X * N.Y,
	        1.092548431f * N.Y * N.Z,
	        .315391565f * (3 * N.Z * N.Z - 1),
	        1.092548431f * N.X * N.Z,
	        .546274215f * (N.X * N.X - N.Y * N.Y)};
}

FVec3 EvaluateEnvironmentSh(const FEnvironmentSh& InSh, FVec3 InNormal)
{
	const auto Basis = EnvironmentShBasis(InNormal);
	FVec3 Result;
	for (unsigned Index = 0; Index < 9; ++Index)
	{
		Result = Add(Result, ScaleVector({InSh[Index][0], InSh[Index][1], InSh[Index][2]}, Basis[Index]));
	}
	return Result;
}

FEnvironmentSh ProjectEnvironmentSh(const FTextureAsset& InCube)
{
	ValidateTextureAsset(InCube);
	if (InCube.Dimension != ETextureDimension::Cube)
	{
		throw std::invalid_argument("SH projection requires a cube");
	}
	std::array<std::array<double, 3>, 9> Sums{};
	const auto& Mip = InCube.Mips.front();
	const auto Size = Mip.Width;
	for (unsigned Face = 0; Face < 6; ++Face)
	{
		for (unsigned Y = 0; Y < Size; ++Y)
		{
			for (unsigned X = 0; X < Size; ++X)
			{
				const double U = 2.0 * X / Size - 1;
				const double V = 2.0 * Y / Size - 1;
				const double Step = 2.0 / Size;
				const double Weight = Area(U + Step, V + Step) - Area(U, V + Step) - Area(U + Step, V) + Area(U, V);
				const auto Basis = EnvironmentShBasis(CubeDirection(Face, float(U + Step / 2), float(V + Step / 2)));
				const auto Pixel = ReadTexturePixel(Mip, InCube.Format, (std::size_t(Face) * Size + Y) * Size + X);
				for (unsigned Index = 0; Index < 9; ++Index)
				{
					for (unsigned Channel = 0; Channel < 3; ++Channel)
					{
						Sums[Index][Channel] += Pixel[Channel] * Basis[Index] * Weight;
					}
				}
			}
		}
	}
	FEnvironmentSh Result{};
	for (unsigned Index = 0; Index < 9; ++Index)
	{
		const double Kernel = std::numbers::pi * (Index == 0 ? 1 : Index < 4 ? 2.0 / 3 : .25);
		for (unsigned Channel = 0; Channel < 3; ++Channel)
		{
			Result[Index][Channel] = float(Sums[Index][Channel] * Kernel);
		}
	}
	return Result;
}
} // namespace Hyperion
