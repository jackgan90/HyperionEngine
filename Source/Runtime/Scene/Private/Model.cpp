#include "Hyperion/Scene/Model.h"
#include <algorithm>
#include <functional>

namespace Hyperion
{
namespace
{
void Require(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

FVec3 VectorAt(const std::vector<float>& InData, std::size_t InIndex)
{
	return {InData[InIndex * 3], InData[InIndex * 3 + 1], InData[InIndex * 3 + 2]};
}

void Include(FBounds& InBounds, FVec3 InPoint)
{
	Require(std::isfinite(InPoint.X) && std::isfinite(InPoint.Y) && std::isfinite(InPoint.Z),
	        "Non-finite transformed model bounds");
	if (!InBounds.bValid)
	{
		InBounds.Minimum = InBounds.Maximum = InPoint;
		InBounds.bValid = true;
	}
	InBounds.Minimum = {std::min(InBounds.Minimum.X, InPoint.X), std::min(InBounds.Minimum.Y, InPoint.Y),
	                    std::min(InBounds.Minimum.Z, InPoint.Z)};
	InBounds.Maximum = {std::max(InBounds.Maximum.X, InPoint.X), std::max(InBounds.Maximum.Y, InPoint.Y),
	                    std::max(InBounds.Maximum.Z, InPoint.Z)};
}
} // namespace

void ValidateNodeHierarchy(std::span<const FModelNode> InNodes)
{
	std::vector<unsigned> Parents(InNodes.size());
	for (const auto& Node : InNodes)
	{
		for (float Value : Node.Local.Values)
		{
			Require(std::isfinite(Value), "Non-finite node transform");
		}
		for (auto Child : Node.Children)
		{
			Require(Child < InNodes.size(), "Invalid child node");
			Require(++Parents[Child] == 1, "Node has multiple parents");
		}
	}
	std::vector<std::pair<std::size_t, unsigned>> Pending;
	Pending.reserve(InNodes.size());
	for (std::size_t Index = 0; Index < InNodes.size(); ++Index)
	{
		if (!Parents[Index])
		{
			Pending.emplace_back(Index, 0);
		}
	}
	for (std::size_t Index = 0; Index < Pending.size(); ++Index)
	{
		const auto Node = Pending[Index].first;
		const auto Depth = Pending[Index].second;
		Require(Depth < 256, "Node hierarchy too deep");
		for (auto Child : InNodes[Node].Children)
		{
			Pending.emplace_back(Child, Depth + 1);
		}
	}
	Require(Pending.size() == InNodes.size(), "Node cycle");
}

void ValidateModel(const FModelAsset& InModel)
{
	Require(!InModel.Roots.empty() && !InModel.Primitives.empty(), "Model has no renderable scene");
	for (const auto& Primitive : InModel.Primitives)
	{
		const auto Count = Primitive.Positions.size() / 3;
		Require(Count && Primitive.Positions.size() % 3 == 0, "Invalid model positions");
		Require(!Primitive.Indices.empty() && Primitive.Indices.size() % 3 == 0, "Invalid triangle indices");
		for (const auto Index : Primitive.Indices)
		{
			Require(Index < Count, "Model index out of range");
		}
		const auto Attribute = [&](const std::vector<float>& InValues, std::size_t InComponents)
		{
			Require(InValues.empty() || InValues.size() == Count * InComponents, "Attribute count mismatch");
			for (float Value : InValues)
			{
				Require(std::isfinite(Value), "Non-finite model attribute");
			}
		};
		Attribute(Primitive.Positions, 3);
		Attribute(Primitive.Normals, 3);
		Attribute(Primitive.Tangents, 4);
		Attribute(Primitive.Colors, 4);
		Attribute(Primitive.TexCoords0, 2);
		Attribute(Primitive.TexCoords1, 2);
		Require(Primitive.Material >= 0 && std::size_t(Primitive.Material) < InModel.MaterialSlots.size(),
		        "Invalid material reference");
	}
	for (const auto& Reference : InModel.MaterialSlots)
	{
		ValidateAssetRef(Reference);
		Require(Reference.TypeId == "hyperion.materialasset", "Model slot requires a material asset reference");
	}
	ValidateNodeHierarchy(InModel.Nodes);
	std::vector<bool> Parents(InModel.Nodes.size());
	for (const auto& Node : InModel.Nodes)
	{
		for (auto Primitive : Node.Primitives)
		{
			Require(Primitive < InModel.Primitives.size(), "Invalid primitive reference");
		}
		for (auto Child : Node.Children)
		{
			Parents[Child] = true;
		}
	}
	std::vector<bool> RootSeen(InModel.Nodes.size());
	for (auto Root : InModel.Roots)
	{
		Require(Root < InModel.Nodes.size(), "Invalid scene root");
		Require(!Parents[Root] && !RootSeen[Root], "Duplicate or parented scene root");
		RootSeen[Root] = true;
	}
}

std::vector<FModelInstance> ModelInstances(const FModelAsset& InModel)
{
	ValidateModel(InModel);
	std::vector<FModelInstance> Instances;
	std::function<void(std::uint32_t, const FMat4&)> Visit = [&](std::uint32_t InNode, const FMat4& InParent)
	{
		const auto& Node = InModel.Nodes[InNode];
		const auto World = Multiply(InParent, Node.Local);
		for (auto Primitive : Node.Primitives)
		{
			Instances.push_back({Primitive, World});
		}
		for (auto Child : Node.Children)
		{
			Visit(Child, World);
		}
	};
	for (auto Root : InModel.Roots)
	{
		Visit(Root, Identity());
	}
	return Instances;
}

FBounds ModelBounds(const FModelAsset& InModel)
{
	FBounds Bounds;
	std::vector<FBounds> LocalBounds(InModel.Primitives.size());
	for (const auto& Instance : ModelInstances(InModel))
	{
		auto& Local = LocalBounds[Instance.Primitive];
		if (!Local.bValid)
		{
			const auto& Positions = InModel.Primitives[Instance.Primitive].Positions;
			for (std::size_t Index = 0; Index < Positions.size(); Index += 3)
			{
				Include(Local, {Positions[Index], Positions[Index + 1], Positions[Index + 2]});
			}
		}
		for (unsigned Corner = 0; Corner < 8; ++Corner)
		{
			const auto Point = Transform(Instance.World, {Corner & 1 ? Local.Maximum.X : Local.Minimum.X,
			                                              Corner & 2 ? Local.Maximum.Y : Local.Minimum.Y,
			                                              Corner & 4 ? Local.Maximum.Z : Local.Minimum.Z, 1});
			Include(Bounds, {Point.X, Point.Y, Point.Z});
		}
	}
	return Bounds;
}

void GenerateMeshDirections(FModelPrimitive& InPrimitive, std::uint32_t InTangentUv)
{
	if (!InPrimitive.Normals.empty() && !InPrimitive.Tangents.empty())
	{
		return;
	}
	const auto Count = InPrimitive.Positions.size() / 3;
	const bool bMissingNormals = InPrimitive.Normals.empty();
	std::vector<FVec3> Normals(Count);
	std::vector<FVec3> Tangents(Count);
	std::vector<FVec3> Bitangents(Count);
	const auto& Uvs = InTangentUv == 1 ? InPrimitive.TexCoords1 : InPrimitive.TexCoords0;
	for (std::size_t Index = 0; Index < InPrimitive.Indices.size(); Index += 3)
	{
		const auto A = InPrimitive.Indices[Index];
		const auto B = InPrimitive.Indices[Index + 1];
		const auto C = InPrimitive.Indices[Index + 2];
		const auto Edge1 = Subtract(VectorAt(InPrimitive.Positions, B), VectorAt(InPrimitive.Positions, A));
		const auto Edge2 = Subtract(VectorAt(InPrimitive.Positions, C), VectorAt(InPrimitive.Positions, A));
		const auto Normal = Cross(Edge1, Edge2);
		for (auto Vertex : {A, B, C})
		{
			Normals[Vertex] = Add(Normals[Vertex], Normal);
		}
		if (!Uvs.empty())
		{
			const float U1 = Uvs[B * 2] - Uvs[A * 2];
			const float V1 = Uvs[B * 2 + 1] - Uvs[A * 2 + 1];
			const float U2 = Uvs[C * 2] - Uvs[A * 2];
			const float V2 = Uvs[C * 2 + 1] - Uvs[A * 2 + 1];
			const float Denominator = U1 * V2 - V1 * U2;
			if (std::abs(Denominator) > 1e-10f)
			{
				const auto Tangent =
				    ScaleVector(Subtract(ScaleVector(Edge1, V2), ScaleVector(Edge2, V1)), 1 / Denominator);
				const auto Bitangent =
				    ScaleVector(Subtract(ScaleVector(Edge2, U1), ScaleVector(Edge1, U2)), 1 / Denominator);
				for (auto Vertex : {A, B, C})
				{
					Tangents[Vertex] = Add(Tangents[Vertex], Tangent);
					Bitangents[Vertex] = Add(Bitangents[Vertex], Bitangent);
				}
			}
		}
	}
	if (bMissingNormals)
	{
		InPrimitive.Normals.resize(Count * 3);
	}
	const bool bMissingTangents = InPrimitive.Tangents.empty();
	if (bMissingTangents)
	{
		InPrimitive.Tangents.resize(Count * 4);
	}
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		const auto Normal = Normalize(bMissingNormals ? Normals[Index] : VectorAt(InPrimitive.Normals, Index));
		if (bMissingNormals)
		{
			InPrimitive.Normals[Index * 3] = Normal.X;
			InPrimitive.Normals[Index * 3 + 1] = Normal.Y;
			InPrimitive.Normals[Index * 3 + 2] = Normal.Z;
		}
		if (bMissingTangents)
		{
			auto Tangent = Subtract(Tangents[Index], ScaleVector(Normal, Dot(Normal, Tangents[Index])));
			if (Length(Tangent) < 1e-10f)
			{
				Tangent = Cross(std::abs(Normal.Y) < .9f ? FVec3{0, 1, 0} : FVec3{1, 0, 0}, Normal);
			}
			Tangent = Normalize(Tangent);
			InPrimitive.Tangents[Index * 4] = Tangent.X;
			InPrimitive.Tangents[Index * 4 + 1] = Tangent.Y;
			InPrimitive.Tangents[Index * 4 + 2] = Tangent.Z;
			InPrimitive.Tangents[Index * 4 + 3] = Dot(Cross(Normal, Tangent), Bitangents[Index]) < 0 ? -1.f : 1.f;
		}
	}
}
} // namespace Hyperion
