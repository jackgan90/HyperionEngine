#include "LightVolumeResources.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Hyperion
{
namespace
{
using FTriangle = std::array<FVec3, 3>;

FDrawPacket Upload(IRHIDevice& InDevice, const std::vector<FTriangle>& InTriangles)
{
	std::vector<FVec3> Vertices;
	std::vector<std::uint32_t> Indices;
	for (const auto& Triangle : InTriangles)
	{
		for (const auto Vertex : Triangle)
		{
			Indices.push_back(static_cast<std::uint32_t>(Vertices.size()));
			Vertices.push_back(Vertex);
		}
	}
	FDrawPacket Result;
	Result.Vertices = InDevice.CreateBuffer(std::as_bytes(std::span(Vertices)));
	Result.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	Result.VertexStride = sizeof(FVec3);
	Result.IndexCount = static_cast<std::uint32_t>(Indices.size());
	return Result;
}

std::vector<FTriangle> Sphere()
{
	std::vector<FTriangle> Triangles;
	for (const float X : {-1.f, 1.f})
	{
		for (const float Y : {-1.f, 1.f})
		{
			for (const float Z : {-1.f, 1.f})
			{
				FTriangle Triangle{{{X, 0, 0}, {0, Y, 0}, {0, 0, Z}}};
				if (X * Y * Z < 0)
				{
					std::swap(Triangle[1], Triangle[2]);
				}
				Triangles.push_back(Triangle);
			}
		}
	}
	for (unsigned Level = 0; Level < 3; ++Level)
	{
		std::vector<FTriangle> Refined;
		for (const auto& Triangle : Triangles)
		{
			const auto A = Normalize(Add(Triangle[0], Triangle[1]));
			const auto B = Normalize(Add(Triangle[1], Triangle[2]));
			const auto C = Normalize(Add(Triangle[2], Triangle[0]));
			Refined.push_back({Triangle[0], A, C});
			Refined.push_back({A, Triangle[1], B});
			Refined.push_back({C, B, Triangle[2]});
			Refined.push_back({A, B, C});
		}
		Triangles = std::move(Refined);
	}
	float Expansion = 1;
	for (const auto& Triangle : Triangles)
	{
		const auto Normal = Normalize(Cross(Subtract(Triangle[1], Triangle[0]), Subtract(Triangle[2], Triangle[0])));
		Expansion = std::max(Expansion, 1.0001f / Dot(Normal, Triangle[0]));
	}
	for (auto& Triangle : Triangles)
	{
		for (auto& Vertex : Triangle)
		{
			Vertex = ScaleVector(Vertex, Expansion);
		}
	}
	return Triangles;
}

std::vector<FTriangle> Cone()
{
	constexpr unsigned Sides = 48;
	constexpr float Step = 2 * std::numbers::pi_v<float> / Sides;
	const float Radius = 1.0001f / std::cos(Step * .5f);
	std::array<FVec3, Sides> Ring;
	for (unsigned Index = 0; Index < Sides; ++Index)
	{
		Ring[Index] = {Radius * std::cos(Index * Step), Radius * std::sin(Index * Step), -1};
	}
	std::vector<FTriangle> Triangles;
	for (unsigned Index = 0; Index < Sides; ++Index)
	{
		const auto A = Ring[Index];
		const auto B = Ring[(Index + 1) % Sides];
		Triangles.push_back({FVec3{}, A, B});
		Triangles.push_back({FVec3{0, 0, -1}, B, A});
	}
	return Triangles;
}
} // namespace

void FLightVolumeResources::Initialize(IRHIDevice& InDevice)
{
	if (!Sphere.Vertices)
	{
		Sphere = Upload(InDevice, Hyperion::Sphere());
	}
	if (!Cone.Vertices)
	{
		Cone = Upload(InDevice, Hyperion::Cone());
	}
}
} // namespace Hyperion
