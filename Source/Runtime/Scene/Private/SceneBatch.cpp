#include "SceneInternal.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
void FSceneStorage::FMutation::StageNode(std::uint32_t InSlot, FSceneNode InNode)
{
	const auto& Before = *Read(InSlot).Node;
	if (InNode.Id != Before.Id)
	{
		throw std::invalid_argument("Scene object identity is immutable");
	}
	ValidateSceneNode(InNode);
	if (InNode == Before)
	{
		return;
	}
	ESceneChangeMask Mask = ESceneChangeMask::Metadata;
	const bool bParentChanged = Before.Parent() != InNode.Parent();
	const auto Parent = InNode.Parent().empty() ? InvalidSlot : Storage.FindId(InNode.Parent());
	if (!InNode.Parent().empty() && Parent == InvalidSlot)
	{
		throw std::invalid_argument("Missing scene parent: " + InNode.Parent());
	}
	if (Before.Local().Values != InNode.Local().Values)
	{
		Mask |= ESceneChangeMask::Transform;
	}
	if (Before.bEnabled != InNode.bEnabled)
	{
		Mask |= ESceneChangeMask::Enabled;
	}
	if (Before.Model() != InNode.Model())
	{
		Mask |= ESceneChangeMask::Model;
	}
	if (Before.Camera() != InNode.Camera())
	{
		Mask |= ESceneChangeMask::Camera;
	}
	if (Before.DirectionalLight() != InNode.DirectionalLight() ||
	    Before.EnvironmentLight() != InNode.EnvironmentLight() || Before.PointLight() != InNode.PointLight() ||
	    Before.SpotLight() != InNode.SpotLight())
	{
		Mask |= ESceneChangeMask::Light;
	}
	Edit(InSlot).Node = std::move(InNode);
	if (bParentChanged)
	{
		Unlink(InSlot);
		Link(InSlot, Parent);
		Mask |= ESceneChangeMask::Structure;
	}
	const auto Handle = Storage.Handle(InSlot);
	const auto& Edited = *Read(InSlot).Node;
	if (Settings.DefaultCamera == Handle && !Edited.Camera())
	{
		Settings.DefaultCamera.reset();
	}
	if (Settings.MainDirectionalLight == Handle && !Edited.DirectionalLight())
	{
		Settings.MainDirectionalLight.reset();
	}
	if (Settings.EnvironmentLight == Handle && !Edited.EnvironmentLight())
	{
		Settings.EnvironmentLight.reset();
	}
	Mark(InSlot, Mask);
}

void FSceneStorage::FMutation::ValidateHierarchy(std::span<const std::uint32_t> InSlots) const
{
	for (const auto Slot : InSlots)
	{
		std::set<std::uint32_t> Ancestors{Slot};
		for (auto Parent = Read(Slot).Parent; Parent != InvalidSlot; Parent = Read(Parent).Parent)
		{
			if (!Ancestors.insert(Parent).second)
			{
				throw std::invalid_argument("Component edit would create a hierarchy cycle");
			}
		}
	}
}

void FSceneStorage::FMutation::DeriveRoots(std::span<const std::uint32_t> InSlots)
{
	const std::set<std::uint32_t> Candidates(InSlots.begin(), InSlots.end());
	for (const auto Slot : InSlots)
	{
		bool bCovered{};
		for (auto Parent = Read(Slot).Parent; Parent != InvalidSlot; Parent = Read(Parent).Parent)
		{
			bCovered |= Candidates.contains(Parent);
		}
		if (!bCovered)
		{
			Derive(Slot);
		}
	}
}

bool FSceneStorage::EditNodes(std::vector<FSceneNodeEdit> InEdits)
{
	FMutation Mutation(*this);
	std::set<std::uint32_t> Seen;
	std::vector<std::uint32_t> Derived;
	for (auto& Edit : InEdits)
	{
		const auto Slot = Live(Edit.Handle);
		if (Slot == InvalidSlot)
		{
			return false;
		}
		if (!Seen.insert(Slot).second)
		{
			throw std::invalid_argument("Duplicate batch edit target");
		}
		const auto& Before = *Slots[Slot]->Node;
		if (Before.Local().Values != Edit.Node.Local().Values || Before.Parent() != Edit.Node.Parent() ||
		    Before.bEnabled != Edit.Node.bEnabled)
		{
			Derived.push_back(Slot);
		}
		Mutation.StageNode(Slot, std::move(Edit.Node));
	}
	Mutation.ValidateHierarchy(Derived);
	Mutation.DeriveRoots(Derived);
	for (const auto Slot : Seen)
	{
		const auto& Entry = Mutation.Read(Slot);
		ValidateSceneWorld(*Entry.Node, Entry.World);
	}
	Mutation.Commit();
	return true;
}

std::vector<FSceneHandle> FSceneStorage::AddNodes(std::vector<FSceneNode> InNodes)
{
	FMutation Mutation(*this);
	std::vector<std::string> Parents;
	std::vector<std::uint32_t> Added;
	std::vector<FSceneHandle> Result;
	for (auto& Node : InNodes)
	{
		Parents.push_back(Node.Parent());
		Added.push_back(Mutation.Add(std::move(Node), InvalidSlot));
	}
	for (std::size_t Index = 0; Index < Added.size(); ++Index)
	{
		const auto& Id = Parents[Index];
		const auto Found = Mutation.AddedIds.find(Id);
		const auto Parent = Id.empty() ? InvalidSlot : Found != Mutation.AddedIds.end() ? Found->second : FindId(Id);
		if (!Id.empty() && Parent == InvalidSlot)
		{
			throw std::invalid_argument("Missing scene parent: " + Id);
		}
		Mutation.Unlink(Added[Index]);
		Mutation.Link(Added[Index], Parent);
		Result.push_back({Identity, Added[Index], Mutation.Read(Added[Index]).Generation});
	}
	Mutation.ValidateHierarchy(Added);
	Mutation.DeriveRoots(Added);
	Mutation.Commit();
	return Result;
}

bool FSceneStorage::RemoveSubtrees(std::span<const FSceneHandle> InHandles)
{
	std::set<std::uint32_t> Removed;
	for (const auto Handle : InHandles)
	{
		const auto Slot = Live(Handle);
		if (Slot == InvalidSlot)
		{
			return false;
		}
		const auto Children = Subtree(Slot);
		Removed.insert(Children.begin(), Children.end());
	}
	FMutation Mutation(*this);
	for (const auto Slot : Removed)
	{
		if (!Removed.contains(Slots[Slot]->Parent))
		{
			Mutation.Unlink(Slot);
		}
		Mutation.Remove(Slot);
	}
	Mutation.Commit();
	return true;
}
} // namespace Hyperion
