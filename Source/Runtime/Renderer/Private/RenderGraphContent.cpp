#include "RenderGraphResources.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
namespace
{
FViewport FullRegion(FSize InSize)
{
	return {0, 0, float(InSize.Width), float(InSize.Height)};
}

std::vector<FViewport> Subtract(FViewport InArea, const FViewport& InCut)
{
	const auto Left = std::max(InArea.X, InCut.X);
	const auto Top = std::max(InArea.Y, InCut.Y);
	const auto Right = std::min(InArea.X + InArea.Width, InCut.X + InCut.Width);
	const auto Bottom = std::min(InArea.Y + InArea.Height, InCut.Y + InCut.Height);
	if (Right <= Left || Bottom <= Top)
	{
		return {InArea};
	}
	std::vector<FViewport> Result;
	if (Top > InArea.Y)
	{
		Result.push_back({InArea.X, InArea.Y, InArea.Width, Top - InArea.Y});
	}
	if (Bottom < InArea.Y + InArea.Height)
	{
		Result.push_back({InArea.X, Bottom, InArea.Width, InArea.Y + InArea.Height - Bottom});
	}
	if (Left > InArea.X)
	{
		Result.push_back({InArea.X, Top, Left - InArea.X, Bottom - Top});
	}
	if (Right < InArea.X + InArea.Width)
	{
		Result.push_back({Right, Top, InArea.X + InArea.Width - Right, Bottom - Top});
	}
	return Result;
}
} // namespace

bool FGraphContent::Contains(const std::optional<FViewport>& InRegion, FSize InSize) const
{
	if (bFull)
	{
		return true;
	}
	if (!InRegion && !InSize.Width)
	{
		return false;
	}
	std::vector<FViewport> Missing{InRegion.value_or(FullRegion(InSize))};
	for (const auto& Region : Regions)
	{
		std::vector<FViewport> Remaining;
		for (const auto& Part : Missing)
		{
			auto Pieces = Subtract(Part, Region);
			Remaining.insert(Remaining.end(), Pieces.begin(), Pieces.end());
		}
		Missing = std::move(Remaining);
	}
	return Missing.empty();
}

void FGraphContent::Invalidate(const std::optional<FViewport>& InRegion, FSize InSize)
{
	if (!InRegion || (bFull && !InSize.Width))
	{
		bFull = false;
		Regions.clear();
		return;
	}
	if (bFull)
	{
		Regions = {FullRegion(InSize)};
		bFull = false;
	}
	std::vector<FViewport> Remaining;
	for (const auto& Region : Regions)
	{
		auto Parts = Subtract(Region, *InRegion);
		Remaining.insert(Remaining.end(), Parts.begin(), Parts.end());
	}
	Regions = std::move(Remaining);
}

void FGraphContent::Load(EAttachmentLoad InLoad, const std::optional<FViewport>& InRegion, FSize InSize)
{
	if (InLoad == EAttachmentLoad::Load)
	{
		if (!Contains(InRegion, InSize))
		{
			throw std::invalid_argument("Graph loads undefined attachment contents");
		}
	}
	else if (InLoad == EAttachmentLoad::Clear)
	{
		if (!InRegion)
		{
			bFull = true;
			Regions.clear();
		}
		else if (!bFull)
		{
			Regions.push_back(*InRegion);
		}
	}
	else
	{
		Invalidate(InRegion, InSize);
	}
}

void FGraphContent::Store(EAttachmentStore InStore, const std::optional<FViewport>& InRegion, FSize InSize)
{
	if (InStore == EAttachmentStore::Discard)
	{
		Invalidate(InRegion, InSize);
	}
}
} // namespace Hyperion
