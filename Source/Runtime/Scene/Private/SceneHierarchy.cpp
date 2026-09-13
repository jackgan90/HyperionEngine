#include "SceneInternal.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
std::uint32_t FSceneStorage::Live(FSceneHandle InHandle) const
{
	if (InHandle.Scene != Identity || InHandle.Slot >= Slots.size())
	{
		return InvalidSlot;
	}
	const auto& Entry = *Slots[InHandle.Slot];
	return Entry.Node && Entry.Generation == InHandle.Generation ? InHandle.Slot : InvalidSlot;
}

std::uint32_t FSceneStorage::FindId(std::string_view InId) const
{
	const auto Found = Ids.find(InId);
	return Found == Ids.end() ? InvalidSlot : Found->second;
}

FSceneHandle FSceneStorage::Handle(std::uint32_t InSlot) const
{
	return {Identity, InSlot, Slots[InSlot]->Generation};
}

std::vector<FSceneHandle> FSceneStorage::Handles(const std::vector<std::uint32_t>& InSlots) const
{
	std::vector<FSceneHandle> Result;
	Result.reserve(InSlots.size());
	for (const auto Slot : InSlots)
	{
		Result.push_back(Handle(Slot));
	}
	return Result;
}

std::vector<std::uint32_t> FSceneStorage::Subtree(std::uint32_t InRoot) const
{
	std::vector<std::uint32_t> Result{InRoot};
	for (std::size_t Index = 0; Index < Result.size(); ++Index)
	{
		const auto& Children = Slots[Result[Index]]->Children;
		Result.insert(Result.end(), Children.begin(), Children.end());
	}
	return Result;
}

FMat4 FSceneStorage::ToLocal(std::uint32_t InParent, const FMat4& InWorld) const
{
	if (!IsAffine(InWorld))
	{
		throw std::invalid_argument("Invalid scene world transform");
	}
	if (InParent == InvalidSlot)
	{
		return InWorld;
	}
	const auto ParentInverse = Inverse(Slots[InParent]->World);
	if (!IsAffine(ParentInverse))
	{
		throw std::invalid_argument("Scene world edit requires an invertible parent");
	}
	const auto Local = Multiply(ParentInverse, InWorld);
	if (!IsAffine(Local))
	{
		throw std::invalid_argument("Scene world edit produced an invalid local transform");
	}
	return Local;
}

FSceneHandle FSceneStorage::AddNode(FSceneNode InNode)
{
	const auto Parent = InNode.Parent.empty() ? InvalidSlot : FindId(InNode.Parent);
	if (!InNode.Parent.empty() && Parent == InvalidSlot)
	{
		throw std::invalid_argument("Missing scene node parent: " + InNode.Parent);
	}
	FMutation Mutation(*this);
	const auto Slot = Mutation.Add(std::move(InNode), Parent);
	Mutation.Derive(Slot);
	const FSceneHandle Result{Identity, Slot, Mutation.Read(Slot).Generation};
	Mutation.Commit();
	return Result;
}

std::vector<FSceneHandle> FSceneStorage::LoadNodes(std::vector<FSceneNode> InNodes)
{
	if (!Order.empty())
	{
		throw std::logic_error("LoadNodes requires an empty scene");
	}
	std::map<std::string, std::size_t, std::less<>> Indices;
	for (std::size_t Index = 0; Index < InNodes.size(); ++Index)
	{
		ValidateSceneNode(InNodes[Index]);
		if (!Indices.emplace(InNodes[Index].Id, Index).second)
		{
			throw std::invalid_argument("Duplicate scene node ID");
		}
	}
	std::vector<std::size_t> Parents(InNodes.size(), InNodes.size());
	for (std::size_t Index = 0; Index < InNodes.size(); ++Index)
	{
		if (InNodes[Index].Parent.empty())
		{
			continue;
		}
		const auto Parent = Indices.find(InNodes[Index].Parent);
		if (Parent == Indices.end())
		{
			throw std::invalid_argument("Missing scene node parent: " + InNodes[Index].Parent);
		}
		Parents[Index] = Parent->second;
	}
	FMutation Mutation(*this);
	std::vector<std::uint32_t> SlotsByInput;
	std::vector<FSceneHandle> Result;
	SlotsByInput.reserve(InNodes.size());
	Result.reserve(InNodes.size());
	for (auto& Node : InNodes)
	{
		SlotsByInput.push_back(Mutation.Add(std::move(Node), InvalidSlot));
	}
	Mutation.Roots = std::vector<std::uint32_t>{};
	for (std::size_t Index = 0; Index < SlotsByInput.size(); ++Index)
	{
		const auto Slot = SlotsByInput[Index];
		const auto Parent = Parents[Index] == InNodes.size() ? InvalidSlot : SlotsByInput[Parents[Index]];
		Mutation.Link(Slot, Parent);
		Result.push_back({Identity, Slot, Mutation.Read(Slot).Generation});
	}
	std::vector<std::uint32_t> Visited = *Mutation.Roots;
	for (std::size_t Index = 0; Index < Visited.size(); ++Index)
	{
		const auto& Children = Mutation.Read(Visited[Index]).Children;
		Visited.insert(Visited.end(), Children.begin(), Children.end());
	}
	if (Visited.size() != InNodes.size())
	{
		throw std::invalid_argument("Scene node hierarchy contains a cycle");
	}
	for (const auto Root : *Mutation.Roots)
	{
		Mutation.Derive(Root);
	}
	Mutation.Commit();
	return Result;
}

bool FSceneStorage::EditNode(FSceneHandle InHandle, FSceneNode InNode)
{
	const auto Slot = Live(InHandle);
	if (Slot == InvalidSlot)
	{
		return false;
	}
	const auto& Before = *Slots[Slot]->Node;
	if (InNode.Id != Before.Id || InNode.Parent != Before.Parent || InNode.GetKind() != Before.GetKind())
	{
		throw std::invalid_argument("Scene node identity and kind are immutable; use Reparent for hierarchy changes");
	}
	ValidateSceneNode(InNode);
	if (InNode == Before)
	{
		return true;
	}
	ESceneChangeMask Mask = ESceneChangeMask::None;
	const bool bTransformChanged = Before.Local.Values != InNode.Local.Values;
	const bool bEnabledChanged = Before.bEnabled != InNode.bEnabled;
	if (bTransformChanged)
	{
		Mask |= ESceneChangeMask::Transform;
	}
	if (bEnabledChanged)
	{
		Mask |= ESceneChangeMask::Enabled;
	}
	if (Before.Name != InNode.Name)
	{
		Mask |= ESceneChangeMask::Metadata;
	}
	if (Before.Model != InNode.Model)
	{
		Mask |= ESceneChangeMask::Model;
	}
	if (Before.Camera != InNode.Camera)
	{
		Mask |= ESceneChangeMask::Camera;
	}
	if (Before.DirectionalLight != InNode.DirectionalLight || Before.EnvironmentLight != InNode.EnvironmentLight ||
	    Before.PointLight != InNode.PointLight || Before.SpotLight != InNode.SpotLight)
	{
		Mask |= ESceneChangeMask::Light;
	}
	FMutation Mutation(*this);
	Mutation.Edit(Slot).Node = std::move(InNode);
	Mutation.Mark(Slot, Mask);
	if (bTransformChanged || bEnabledChanged)
	{
		Mutation.Derive(Slot);
	}
	else
	{
		ValidateSceneWorld(*Mutation.Read(Slot).Node, Mutation.Read(Slot).World);
	}
	Mutation.Commit();
	return true;
}

bool FSceneStorage::Reparent(FSceneHandle InHandle, std::optional<FSceneHandle> InParent, ESceneReparentMode InMode)
{
	const auto Slot = Live(InHandle);
	if (Slot == InvalidSlot)
	{
		return false;
	}
	if (InMode != ESceneReparentMode::KeepLocal && InMode != ESceneReparentMode::KeepWorld)
	{
		throw std::invalid_argument("Invalid scene reparent mode");
	}
	const auto Parent = InParent ? Live(*InParent) : InvalidSlot;
	if (InParent && Parent == InvalidSlot)
	{
		throw std::invalid_argument("Stale or foreign scene parent");
	}
	for (auto Ancestor = Parent; Ancestor != InvalidSlot; Ancestor = Slots[Ancestor]->Parent)
	{
		if (Ancestor == Slot)
		{
			throw std::invalid_argument("Scene reparent would create a cycle");
		}
	}
	if (Parent == Slots[Slot]->Parent)
	{
		return true;
	}
	const auto Local =
	    InMode == ESceneReparentMode::KeepWorld ? ToLocal(Parent, Slots[Slot]->World) : Slots[Slot]->Node->Local;
	FMutation Mutation(*this);
	Mutation.Unlink(Slot);
	Mutation.Link(Slot, Parent);
	Mutation.Edit(Slot).Node->Local = Local;
	Mutation.Mark(Slot, ESceneChangeMask::Structure);
	Mutation.Derive(Slot);
	Mutation.Commit();
	return true;
}

bool FSceneStorage::RemoveNodes(FSceneHandle InHandle, bool bInKeepChildren)
{
	const auto Slot = Live(InHandle);
	if (Slot == InvalidSlot)
	{
		return false;
	}
	FMutation Mutation(*this);
	Mutation.Unlink(Slot);
	if (bInKeepChildren)
	{
		const auto Parent = Slots[Slot]->Parent;
		for (const auto Child : Slots[Slot]->Children)
		{
			const auto Local = ToLocal(Parent, Slots[Child]->World);
			Mutation.Unlink(Child);
			Mutation.Link(Child, Parent);
			Mutation.Edit(Child).Node->Local = Local;
			Mutation.Mark(Child, ESceneChangeMask::Structure);
			Mutation.Derive(Child);
		}
		Mutation.Remove(Slot);
	}
	else
	{
		for (const auto Child : Subtree(Slot))
		{
			Mutation.Remove(Child);
		}
	}
	Mutation.Commit();
	return true;
}

void FSceneStorage::ValidateSettings(const FSceneSettings& InSettings) const
{
	const auto Validate = [&](const std::optional<FSceneHandle>& InHandle, ESceneNodeKind InKind)
	{
		if (!InHandle)
		{
			return;
		}
		const auto Slot = Live(*InHandle);
		if (Slot == InvalidSlot || Slots[Slot]->Node->GetKind() != InKind)
		{
			throw std::invalid_argument(std::string("Scene selection requires a live ") + ToString(InKind));
		}
	};
	Validate(InSettings.DefaultCamera, ESceneNodeKind::Camera);
	Validate(InSettings.MainDirectionalLight, ESceneNodeKind::DirectionalLight);
	Validate(InSettings.EnvironmentLight, ESceneNodeKind::EnvironmentLight);
}
} // namespace Hyperion
