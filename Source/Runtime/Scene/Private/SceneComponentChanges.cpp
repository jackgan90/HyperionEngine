#include "SceneComponentChanges.h"
#include <tuple>

namespace Hyperion
{
ESceneComponentChangeFlags operator|(ESceneComponentChangeFlags InA, ESceneComponentChangeFlags InB)
{
	return static_cast<ESceneComponentChangeFlags>(static_cast<std::uint8_t>(InA) | static_cast<std::uint8_t>(InB));
}

ESceneComponentChangeFlags& operator|=(ESceneComponentChangeFlags& InA, ESceneComponentChangeFlags InB)
{
	InA = InA | InB;
	return InA;
}

bool HasComponentChange(ESceneComponentChangeFlags InFlags, ESceneComponentChangeFlags InChanges)
{
	return (static_cast<std::uint8_t>(InFlags) & static_cast<std::uint8_t>(InChanges)) != 0;
}

namespace
{
using FComponentIdentity = std::pair<std::string_view, std::string_view>;

struct FSceneComponentPair
{
	const FSceneComponent* Before{};
	const FSceneComponent* After{};
};

using FComponentPairs = std::map<FComponentIdentity, FSceneComponentPair>;

void CollectComponents(FComponentPairs& InPairs, const FSceneNode* InNode, bool bInAfter)
{
	if (!InNode)
	{
		return;
	}
	for (const auto& Component : InNode->Components.All())
	{
		if (Component.Get())
		{
			const auto Found =
			    InPairs.emplace(FComponentIdentity{Component.Type->Id, Component.Id}, FSceneComponentPair{}).first;
			(bInAfter ? Found->second.After : Found->second.Before) = &Component;
		}
	}
}

ESceneChangeMask ComponentEffect(const FSceneComponentDescriptor& InType)
{
	if (InType.CppType == typeid(FSceneModelComponent))
	{
		return ESceneChangeMask::Model;
	}
	if (InType.CppType == typeid(FSceneCamera))
	{
		return ESceneChangeMask::Camera;
	}
	if (InType.CppType == typeid(FSceneDirectionalLight) || InType.CppType == typeid(FSceneEnvironmentLight) ||
	    InType.CppType == typeid(FScenePointLight) || InType.CppType == typeid(FSceneSpotLight))
	{
		return ESceneChangeMask::Light;
	}
	return ESceneChangeMask::None;
}

ESceneChangeMask SemanticEffects(const FSceneNode& InBefore, const FSceneNode& InAfter, ESceneChangeMask InChanged)
{
	ESceneChangeMask Result = ESceneChangeMask::None;
	if (HasChange(InChanged, ESceneChangeMask::Model) && InBefore.Model() != InAfter.Model())
	{
		Result |= ESceneChangeMask::Model;
	}
	if (HasChange(InChanged, ESceneChangeMask::Camera) && InBefore.Camera() != InAfter.Camera())
	{
		Result |= ESceneChangeMask::Camera;
	}
	if (HasChange(InChanged, ESceneChangeMask::Light) &&
	    (InBefore.DirectionalLight() != InAfter.DirectionalLight() ||
	     InBefore.EnvironmentLight() != InAfter.EnvironmentLight() || InBefore.PointLight() != InAfter.PointLight() ||
	     InBefore.SpotLight() != InAfter.SpotLight()))
	{
		Result |= ESceneChangeMask::Light;
	}
	return Result;
}

auto IdentityOf(const FSceneComponentChange& InChange)
{
	return std::tie(InChange.TypeId, InChange.InstanceId);
}
} // namespace

FSceneComponentDifference BuildSceneComponentDifference(const FSceneNode* InBefore, const FSceneNode* InAfter,
                                                        bool bInInitialSync)
{
	FComponentPairs Pairs;
	CollectComponents(Pairs, bInInitialSync ? nullptr : InBefore, false);
	CollectComponents(Pairs, InAfter, true);
	FSceneComponentDifference Result;
	ESceneChangeMask Changed = ESceneChangeMask::None;
	for (const auto& [Identity, Pair] : Pairs)
	{
		ESceneComponentChangeFlags Flags = ESceneComponentChangeFlags::None;
		if (!Pair.Before)
		{
			Flags = ESceneComponentChangeFlags::Added;
		}
		else if (!Pair.After)
		{
			Flags = ESceneComponentChangeFlags::Removed;
		}
		else if (!Pair.Before->Type->Equal(Pair.Before->State, Pair.After->State))
		{
			Flags = ESceneComponentChangeFlags::Modified;
		}
		if (Flags != ESceneComponentChangeFlags::None)
		{
			Result.Changes.push_back({std::string(Identity.first), std::string(Identity.second), Flags});
			Changed |= ComponentEffect(*(Pair.After ? Pair.After : Pair.Before)->Type);
		}
	}
	// Structure already covers whole-node admission/removal and initial synchronization.
	if (InBefore && InAfter && !bInInitialSync)
	{
		Result.Effects = SemanticEffects(*InBefore, *InAfter, Changed);
	}
	return Result;
}

std::vector<FSceneComponentChange> MergeSceneComponentChanges(std::span<const FSceneComponentChange> InBefore,
                                                              std::span<const FSceneComponentChange> InAfter)
{
	std::vector<FSceneComponentChange> Result;
	Result.reserve(InBefore.size() + InAfter.size());
	std::size_t BeforeIndex{};
	std::size_t AfterIndex{};
	while (BeforeIndex < InBefore.size() || AfterIndex < InAfter.size())
	{
		if (AfterIndex == InAfter.size() ||
		    (BeforeIndex < InBefore.size() && IdentityOf(InBefore[BeforeIndex]) < IdentityOf(InAfter[AfterIndex])))
		{
			Result.push_back(InBefore[BeforeIndex++]);
		}
		else if (BeforeIndex == InBefore.size() || IdentityOf(InAfter[AfterIndex]) < IdentityOf(InBefore[BeforeIndex]))
		{
			Result.push_back(InAfter[AfterIndex++]);
		}
		else
		{
			auto Change = InBefore[BeforeIndex++];
			Change.Flags |= InAfter[AfterIndex++].Flags;
			Result.push_back(std::move(Change));
		}
	}
	return Result;
}
} // namespace Hyperion
