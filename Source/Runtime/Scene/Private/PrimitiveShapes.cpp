#include "Hyperion/Scene/PrimitiveShapes.h"
#include <array>
#include <cmath>
#include <numbers>

namespace Hyperion
{
namespace
{
constexpr unsigned Segments = 32;
constexpr float Pi = std::numbers::pi_v<float>;

std::uint32_t AddVertex(FModelPrimitive& InMesh, FVec3 InPosition, FVec3 InNormal, FVec2 InUv)
{
	const auto Index = static_cast<std::uint32_t>(InMesh.Positions.size() / 3);
	InMesh.Positions.insert(InMesh.Positions.end(), {InPosition.X, InPosition.Y, InPosition.Z});
	InMesh.Normals.insert(InMesh.Normals.end(), {InNormal.X, InNormal.Y, InNormal.Z});
	InMesh.TexCoords0.insert(InMesh.TexCoords0.end(), {InUv.X, InUv.Y});
	return Index;
}

void AddFace(FModelPrimitive& InMesh, FVec3 InNormal, FVec3 InU, FVec3 InV, float InOffset)
{
	const auto Start = static_cast<std::uint32_t>(InMesh.Positions.size() / 3);
	for (const FVec2 Uv : {FVec2{0, 0}, FVec2{1, 0}, FVec2{1, 1}, FVec2{0, 1}})
	{
		const auto Position =
		    Add(ScaleVector(InNormal, InOffset), Add(ScaleVector(InU, Uv.X - .5f), ScaleVector(InV, Uv.Y - .5f)));
		AddVertex(InMesh, Position, InNormal, Uv);
	}
	for (const unsigned Corner : {0u, 1u, 2u, 0u, 2u, 3u})
	{
		InMesh.Indices.push_back(Start + Corner);
	}
}

void Sphere(FModelPrimitive& InMesh)
{
	constexpr unsigned Rings = 16;
	for (unsigned Y = 0; Y <= Rings; ++Y)
	{
		const float Latitude = Pi * float(Y) / Rings;
		for (unsigned X = 0; X <= Segments; ++X)
		{
			const float Longitude = 2 * Pi * float(X) / Segments;
			const FVec3 Normal{std::sin(Latitude) * std::cos(Longitude), std::cos(Latitude),
			                   std::sin(Latitude) * std::sin(Longitude)};
			AddVertex(InMesh, ScaleVector(Normal, .5f), Normal, {float(X) / Segments, float(Y) / Rings});
		}
	}
	for (unsigned Y = 0; Y < Rings; ++Y)
	{
		for (unsigned X = 0; X < Segments; ++X)
		{
			const auto A = Y * (Segments + 1) + X;
			const auto B = A + Segments + 1;
			if (Y > 0)
			{
				InMesh.Indices.insert(InMesh.Indices.end(), {A, A + 1, B});
			}
			if (Y + 1 < Rings)
			{
				InMesh.Indices.insert(InMesh.Indices.end(), {A + 1, B + 1, B});
			}
		}
	}
}

void Cap(FModelPrimitive& InMesh, float InY, float InNormal)
{
	const auto Center = AddVertex(InMesh, {0, InY, 0}, {0, InNormal, 0}, {.5f, .5f});
	for (unsigned Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = 2 * Pi * float(Index) / Segments;
		const float X = .5f * std::cos(Angle);
		const float Z = .5f * std::sin(Angle);
		AddVertex(InMesh, {X, InY, Z}, {0, InNormal, 0}, {X + .5f, Z + .5f});
		if (Index)
		{
			const auto A = Center + Index;
			const auto B = A + 1;
			InMesh.Indices.insert(InMesh.Indices.end(), {Center, InNormal > 0 ? B : A, InNormal > 0 ? A : B});
		}
	}
}

void RoundSides(FModelPrimitive& InMesh, bool bInCone)
{
	for (unsigned Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = 2 * Pi * float(Index) / Segments;
		const FVec3 Normal = Normalize(FVec3{std::cos(Angle), bInCone ? .5f : 0.f, std::sin(Angle)});
		AddVertex(InMesh, {.5f * std::cos(Angle), -.5f, .5f * std::sin(Angle)}, Normal, {float(Index) / Segments, 1});
		const float Radius = bInCone ? 0.f : .5f;
		AddVertex(InMesh, {Radius * std::cos(Angle), .5f, Radius * std::sin(Angle)}, Normal,
		          {float(Index) / Segments, 0});
		if (Index < Segments)
		{
			const auto A = Index * 2;
			InMesh.Indices.insert(InMesh.Indices.end(), {A, A + 1, A + 2});
			if (!bInCone)
			{
				InMesh.Indices.insert(InMesh.Indices.end(), {A + 1, A + 3, A + 2});
			}
		}
	}
	Cap(InMesh, -.5f, -1);
	if (!bInCone)
	{
		Cap(InMesh, .5f, 1);
	}
}
} // namespace

FModelPrimitive MakePrimitiveShape(EPrimitiveShape InShape)
{
	constexpr std::array Names{"Cube", "Sphere", "Cylinder", "Cone", "Plane"};
	FModelPrimitive Mesh;
	Mesh.Name = Names.at(static_cast<std::size_t>(InShape));
	Mesh.Id = Mesh.Name;
	Mesh.Material = 0;
	switch (InShape)
	{
		case EPrimitiveShape::Cube:
			AddFace(Mesh, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}, .5f);
			AddFace(Mesh, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, .5f);
			AddFace(Mesh, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, .5f);
			AddFace(Mesh, {0, -1, 0}, {1, 0, 0}, {0, 0, 1}, .5f);
			AddFace(Mesh, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, .5f);
			AddFace(Mesh, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, .5f);
			break;
		case EPrimitiveShape::Sphere:
			Sphere(Mesh);
			break;
		case EPrimitiveShape::Cylinder:
		case EPrimitiveShape::Cone:
			RoundSides(Mesh, InShape == EPrimitiveShape::Cone);
			break;
		case EPrimitiveShape::Plane:
			AddFace(Mesh, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, 0);
			break;
	}
	GenerateMeshDirections(Mesh);
	return Mesh;
}
} // namespace Hyperion
