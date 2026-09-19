#include "Hyperion/Math/StaticBoundsBvh.h"
#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace Hyperion
{
FStaticBoundsBvh::FStaticBoundsBvh(std::span<const FBounds> InBounds, const std::function<void()>& InCheckCancellation)
{
	if (InBounds.size() > std::numeric_limits<std::uint32_t>::max() / 2)
	{
		throw std::length_error("Static BVH capacity exceeded");
	}
	Items.resize(InBounds.size());
	std::iota(Items.begin(), Items.end(), 0u);
	if (!Items.empty())
	{
		Nodes.reserve(Items.size() / 2 + 1);
		Build(InBounds, 0, static_cast<std::uint32_t>(Items.size()), InCheckCancellation);
	}
}

std::uint32_t FStaticBoundsBvh::Build(std::span<const FBounds> InBounds, std::uint32_t InBegin, std::uint32_t InEnd,
                                      const std::function<void()>& InCheckCancellation)
{
	if (InCheckCancellation)
	{
		InCheckCancellation();
	}
	FBounds Bounds = InBounds[Items[InBegin]];
	for (auto Index = InBegin + 1; Index < InEnd; ++Index)
	{
		Bounds = UnionBounds(Bounds, InBounds[Items[Index]]);
	}
	const auto Node = static_cast<std::uint32_t>(Nodes.size());
	Nodes.push_back({Bounds, InBegin, InEnd - InBegin});
	if (InEnd - InBegin <= 8)
	{
		return Node;
	}
	const auto Extent = Subtract(Bounds.Maximum, Bounds.Minimum);
	const unsigned Axis = Extent.X >= Extent.Y && Extent.X >= Extent.Z ? 0 : Extent.Y >= Extent.Z ? 1 : 2;
	const auto Middle = InBegin + (InEnd - InBegin) / 2;
	std::nth_element(
	    Items.begin() + InBegin, Items.begin() + Middle, Items.begin() + InEnd,
	    [&](std::uint32_t InA, std::uint32_t InB)
	    {
		    const auto& A = InBounds[InA];
		    const auto& B = InBounds[InB];
		    const std::array<double, 3> CenterA{double(A.Minimum.X) + A.Maximum.X, double(A.Minimum.Y) + A.Maximum.Y,
		                                        double(A.Minimum.Z) + A.Maximum.Z};
		    const std::array<double, 3> CenterB{double(B.Minimum.X) + B.Maximum.X, double(B.Minimum.Y) + B.Maximum.Y,
		                                        double(B.Minimum.Z) + B.Maximum.Z};
		    return CenterA[Axis] == CenterB[Axis] ? InA < InB : CenterA[Axis] < CenterB[Axis];
	    });
	const auto Left = Build(InBounds, InBegin, Middle, InCheckCancellation);
	const auto Right = Build(InBounds, Middle, InEnd, InCheckCancellation);
	Nodes[Node].Left = Left;
	Nodes[Node].Right = Right;
	Nodes[Node].Count = 0;
	return Node;
}

void FStaticBoundsBvh::Raycast(FRay InRay, const std::function<void(std::uint32_t, float&)>& InVisit,
                               std::size_t& OutVisitedNodes) const
{
	std::vector<std::uint32_t> Stack;
	if (!Nodes.empty() && IsUsable(InRay))
	{
		Stack.push_back(0);
	}
	while (!Stack.empty())
	{
		const auto& Node = Nodes[Stack.back()];
		Stack.pop_back();
		++OutVisitedNodes;
		if (!IntersectRayBounds(InRay, Node.Bounds))
		{
			continue;
		}
		if (Node.Count)
		{
			for (auto Index = Node.Begin; Index < Node.Begin + Node.Count; ++Index)
			{
				InVisit(Items[Index], InRay.Maximum);
			}
			continue;
		}
		const auto Left = IntersectRayBounds(InRay, Nodes[Node.Left].Bounds);
		const auto Right = IntersectRayBounds(InRay, Nodes[Node.Right].Bounds);
		if (Left && Right)
		{
			Stack.push_back(*Left <= *Right ? Node.Right : Node.Left);
			Stack.push_back(*Left <= *Right ? Node.Left : Node.Right);
		}
		else if (Left || Right)
		{
			Stack.push_back(Left ? Node.Left : Node.Right);
		}
	}
}

std::size_t FStaticBoundsBvh::GetStorageBytes() const
{
	return Nodes.capacity() * sizeof(FNode) + Items.capacity() * sizeof(std::uint32_t);
}
} // namespace Hyperion
