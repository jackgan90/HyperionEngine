#include "Hyperion/Scene/Model.h"
#include "Hyperion/Serialization/Archive.h"
#include "Support/TestSupport.h"
#include <iostream>

int main()
{
	using namespace Hyperion;
	try
	{
		FModelAsset Model;
		FModelPrimitive Primitive;
		Primitive.Positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
		Primitive.Indices = {0, 1, 2};
		GenerateMeshDirections(Primitive);
		HYP_CHECK(Primitive.Normals[2] == 1);
		Model.Primitives.push_back(Primitive);
		Model.Nodes = {{"parent", Translation({2, 0, 0}), {}, {1, 2}},
		               {"first", Scale({2, 3, 1}), {0}, {}},
		               {"second", Translation({0, 4, 0}), {0}, {}}};
		Model.Roots = {0};
		ValidateModel(Model);
		auto Bounds = ModelBounds(Model);
		HYP_CHECK(Bounds.Valid && Bounds.Minimum.X == 2 && Bounds.Maximum.X == 4 && Bounds.Maximum.Y == 5);
		auto Instances = ModelInstances(Model);
		HYP_CHECK(Instances.size() == 2 && Instances[0].Primitive == Instances[1].Primitive);
		auto Rotated = Model;
		Rotated.Nodes[1].Local = ComposeTRS({-2, 1, 0}, {0, 0, .38268343f, .92387953f}, {-2, 3, 1});
		const auto RotatedBounds = ModelBounds(Rotated);
		for (const auto& Instance : ModelInstances(Rotated))
		{
			const auto& Positions = Rotated.Primitives[Instance.Primitive].Positions;
			for (std::size_t Index = 0; Index < Positions.size(); Index += 3)
			{
				const auto Vertex =
				    Transform(Instance.World, {Positions[Index], Positions[Index + 1], Positions[Index + 2], 1});
				HYP_CHECK(Vertex.X >= RotatedBounds.Minimum.X - .00001f &&
				          Vertex.X <= RotatedBounds.Maximum.X + .00001f);
				HYP_CHECK(Vertex.Y >= RotatedBounds.Minimum.Y - .00001f &&
				          Vertex.Y <= RotatedBounds.Maximum.Y + .00001f);
				HYP_CHECK(Vertex.Z >= RotatedBounds.Minimum.Z - .00001f &&
				          Vertex.Z <= RotatedBounds.Maximum.Z + .00001f);
			}
		}
		auto Restored = Deserialize<FModelAsset>(Serialize(Model));
		HYP_CHECK(Serialize(Restored) == Serialize(Model));
		auto Invalid = WriteValue(Model);
		auto& Fields =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Invalid.Value).at("fields").Value);
		Fields["roots"] = WriteValue(std::vector<std::uint32_t>{999});
		bool InvalidRejected = false;
		try
		{
			Restored = Deserialize<FModelAsset>(EncodeArchive(Invalid));
		}
		catch (...)
		{
			InvalidRejected = true;
		}
		HYP_CHECK(InvalidRejected && Serialize(Restored) == Serialize(Model));
		auto Projection = Perspective(1, 1, .1f, 100);
		auto Near = Transform(Projection, {0, 0, -.1f, 1});
		HYP_CHECK(std::abs(Near.Z / Near.W) < .00001f);
		auto Point = Transform(ComposeTRS({1, 2, 3}, {0, 0, 0, 1}, {2, 3, 4}), {1, 1, 1, 1});
		HYP_CHECK(Point.X == 3 && Point.Y == 5 && Point.Z == 7);
		auto Reversed = Model;
		Reversed.Nodes.clear();
		for (std::uint32_t Index = 0; Index < 300; ++Index)
		{
			FModelNode Node;
			if (Index)
			{
				Node.Children = {Index - 1};
			}
			else
			{
				Node.Primitives = {0};
			}
			Reversed.Nodes.push_back(std::move(Node));
		}
		Reversed.Roots = {299};
		bool DepthRejected = false;
		try
		{
			ValidateModel(Reversed);
		}
		catch (...)
		{
			DepthRejected = true;
		}
		HYP_CHECK(DepthRejected);
		Model.Nodes[1].Children = {0};
		bool Failed = false;
		try
		{
			ValidateModel(Model);
		}
		catch (...)
		{
			Failed = true;
		}
		HYP_CHECK(Failed);
		std::cout << "Static model data checks passed\n";
	}
	catch (const std::exception& InError)
	{
		std::cerr << InError.what() << '\n';
		return 1;
	}
}
