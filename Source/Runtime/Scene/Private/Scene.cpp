#include "SceneInternal.h"
#include <algorithm>
#include <atomic>
#include <stdexcept>

namespace Hyperion
{
FScene::FScene() : Storage(std::make_unique<FSceneStorage>())
{
	static std::atomic_uint64_t NextIdentity{1};
	Storage->Identity = NextIdentity.fetch_add(1);
}

FScene::~FScene() = default;

void FScene::RequireMain() const
{
	if (Storage->Owner != std::this_thread::get_id())
	{
		throw std::logic_error("Logical scene accessed outside its Main owner");
	}
}

std::uint64_t FScene::GetIdentity() const
{
	RequireMain();
	return Storage->Identity;
}

std::uint64_t FScene::GetRevision() const
{
	RequireMain();
	return Storage->Revision;
}

void FScene::Update()
{
	RequireMain();
}

FSceneHandle FScene::AddNode(FSceneNode InNode)
{
	RequireMain();
	return Storage->AddNode(std::move(InNode));
}

std::vector<FSceneHandle> FScene::LoadNodes(std::vector<FSceneNode> InNodes)
{
	RequireMain();
	return Storage->LoadNodes(std::move(InNodes));
}

bool FScene::RemoveSubtree(FSceneHandle InHandle)
{
	RequireMain();
	return Storage->RemoveNodes(InHandle, false);
}

bool FScene::RemoveNodeKeepChildren(FSceneHandle InHandle)
{
	RequireMain();
	return Storage->RemoveNodes(InHandle, true);
}

const FSceneNode* FScene::FindNode(FSceneHandle InHandle) const
{
	RequireMain();
	const auto Slot = Storage->Live(InHandle);
	return Slot == FSceneStorage::InvalidSlot ? nullptr : &*Storage->Slots[Slot]->Node;
}

const FSceneNode* FScene::FindNode(std::string_view InId) const
{
	RequireMain();
	const auto Slot = Storage->FindId(InId);
	return Slot == FSceneStorage::InvalidSlot ? nullptr : &*Storage->Slots[Slot]->Node;
}

FSceneHandle FScene::FindHandle(std::string_view InId) const
{
	RequireMain();
	const auto Slot = Storage->FindId(InId);
	return Slot == FSceneStorage::InvalidSlot ? FSceneHandle{} : Storage->Handle(Slot);
}

bool FScene::GetNodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const
{
	RequireMain();
	const auto Slot = Storage->Live(InHandle);
	if (Slot == FSceneStorage::InvalidSlot)
	{
		return false;
	}
	const auto& Entry = *Storage->Slots[Slot];
	OutView = {InHandle, &*Entry.Node, Entry.World, Entry.bEffectiveEnabled};
	return true;
}

bool FScene::GetWorld(FSceneHandle InHandle, FMat4& OutWorld) const
{
	RequireMain();
	const auto Slot = Storage->Live(InHandle);
	if (Slot == FSceneStorage::InvalidSlot)
	{
		return false;
	}
	OutWorld = Storage->Slots[Slot]->World;
	return true;
}

bool FScene::IsEffectivelyEnabled(FSceneHandle InHandle) const
{
	RequireMain();
	const auto Slot = Storage->Live(InHandle);
	return Slot != FSceneStorage::InvalidSlot && Storage->Slots[Slot]->bEffectiveEnabled;
}

const FSceneCamera* FScene::FindCamera(FSceneHandle InHandle) const
{
	const auto* Node = FindNode(InHandle);
	return Node && Node->Camera ? &*Node->Camera : nullptr;
}

const FSceneDirectionalLight* FScene::FindDirectionalLight(FSceneHandle InHandle) const
{
	const auto* Node = FindNode(InHandle);
	return Node && Node->DirectionalLight ? &*Node->DirectionalLight : nullptr;
}

const FSceneEnvironmentLight* FScene::FindEnvironmentLight(FSceneHandle InHandle) const
{
	const auto* Node = FindNode(InHandle);
	return Node && Node->EnvironmentLight ? &*Node->EnvironmentLight : nullptr;
}

const FSceneModelComponent* FScene::FindModelComponent(FSceneHandle InHandle) const
{
	const auto* Node = FindNode(InHandle);
	return Node && Node->Model ? &*Node->Model : nullptr;
}

bool FScene::GetCameraPose(FSceneHandle InHandle, FSceneCameraPose& OutPose) const
{
	RequireMain();
	const auto Slot = Storage->Live(InHandle);
	return Slot != FSceneStorage::InvalidSlot && Storage->Slots[Slot]->Node->Camera &&
	       TryExtractScenePose(Storage->Slots[Slot]->World, OutPose);
}

std::vector<FSceneHandle> FScene::GetNodes() const
{
	RequireMain();
	return Storage->Handles(Storage->Order);
}

std::vector<FSceneHandle> FScene::GetNodes(ESceneNodeKind InKind) const
{
	RequireMain();
	std::vector<FSceneHandle> Result;
	for (const auto Slot : Storage->Order)
	{
		if (Storage->Slots[Slot]->Node->GetKind() == InKind)
		{
			Result.push_back(Storage->Handle(Slot));
		}
	}
	return Result;
}

std::vector<FSceneHandle> FScene::GetRoots() const
{
	RequireMain();
	return Storage->Handles(Storage->Roots);
}

std::vector<FSceneHandle> FScene::GetChildren(FSceneHandle InHandle) const
{
	RequireMain();
	const auto Slot = Storage->Live(InHandle);
	return Slot == FSceneStorage::InvalidSlot ? std::vector<FSceneHandle>{}
	                                          : Storage->Handles(Storage->Slots[Slot]->Children);
}

std::size_t FScene::CountNodes(ESceneNodeKind InKind) const
{
	RequireMain();
	return Storage->Counts.at(static_cast<std::size_t>(InKind));
}

bool FScene::SetName(FSceneHandle InHandle, std::string InName)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (Existing->Name == InName)
	{
		return true;
	}
	auto Node = *Existing;
	Node.Name = std::move(InName);
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::SetEnabled(FSceneHandle InHandle, bool bInEnabled)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (Existing->bEnabled == bInEnabled)
	{
		return true;
	}
	auto Node = *Existing;
	Node.bEnabled = bInEnabled;
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::SetModelVisible(FSceneHandle InHandle, bool bInVisible)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (!Existing->Model)
	{
		throw std::invalid_argument("Scene node is not a model");
	}
	if (Existing->Model->bVisible == bInVisible)
	{
		return true;
	}
	auto Node = *Existing;
	Node.Model->bVisible = bInVisible;
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::SetModelComponent(FSceneHandle InHandle, FSceneModelComponent InValue)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (!Existing->Model)
	{
		throw std::invalid_argument("Scene node has the wrong payload kind");
	}
	auto Node = *Existing;
	Node.Model = std::move(InValue);
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::SetCameraView(FSceneHandle InHandle, FMat4 InWorld, FSceneCamera InCamera)
{
	RequireMain();
	const auto Slot = Storage->Live(InHandle);
	if (Slot == FSceneStorage::InvalidSlot)
	{
		return false;
	}
	auto Node = *Storage->Slots[Slot]->Node;
	if (!Node.Camera)
	{
		throw std::invalid_argument("Scene node has the wrong payload kind");
	}
	Node.Camera = InCamera;
	if (Storage->Slots[Slot]->World.Values != InWorld.Values)
	{
		Node.Local = Storage->ToLocal(Storage->Slots[Slot]->Parent, InWorld);
	}
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::SetCamera(FSceneHandle InHandle, FSceneCamera InValue)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (!Existing->Camera)
	{
		throw std::invalid_argument("Scene node has the wrong payload kind");
	}
	auto Node = *Existing;
	Node.Camera = std::move(InValue);
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::SetDirectionalLight(FSceneHandle InHandle, FSceneDirectionalLight InValue)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (!Existing->DirectionalLight)
	{
		throw std::invalid_argument("Scene node has the wrong payload kind");
	}
	auto Node = *Existing;
	Node.DirectionalLight = std::move(InValue);
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::SetEnvironmentLight(FSceneHandle InHandle, FSceneEnvironmentLight InValue)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (!Existing->EnvironmentLight)
	{
		throw std::invalid_argument("Scene node has the wrong payload kind");
	}
	auto Node = *Existing;
	Node.EnvironmentLight = std::move(InValue);
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::SetLocalTransform(FSceneHandle InHandle, FMat4 InLocal)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing)
	{
		return false;
	}
	if (Existing->Local.Values == InLocal.Values)
	{
		return true;
	}
	auto Node = *Existing;
	Node.Local = InLocal;
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::SetWorldTransform(FSceneHandle InHandle, FMat4 InWorld)
{
	RequireMain();
	const auto Slot = Storage->Live(InHandle);
	if (Slot == FSceneStorage::InvalidSlot)
	{
		return false;
	}
	if (Storage->Slots[Slot]->World.Values == InWorld.Values)
	{
		return true;
	}
	return SetLocalTransform(InHandle, Storage->ToLocal(Storage->Slots[Slot]->Parent, InWorld));
}

bool FScene::Reparent(FSceneHandle InHandle, std::optional<FSceneHandle> InParent, ESceneReparentMode InMode)
{
	RequireMain();
	return Storage->Reparent(InHandle, InParent, InMode);
}

bool FScene::SetSettings(FSceneSettings InSettings)
{
	RequireMain();
	Storage->ValidateSettings(InSettings);
	if (InSettings == Storage->Settings)
	{
		return true;
	}
	FSceneStorage::FMutation Mutation(*Storage);
	Mutation.Settings = InSettings;
	Mutation.Commit();
	return true;
}

const FSceneSettings& FScene::GetSettings() const
{
	RequireMain();
	return Storage->Settings;
}

std::vector<FSceneChange> FScene::GetChanges() const
{
	RequireMain();
	std::vector<FSceneChange> Result;
	Result.reserve(Storage->Changes.size());
	for (const auto& [Handle, Change] : Storage->Changes)
	{
		Result.push_back(Change);
	}
	std::stable_sort(Result.begin(), Result.end(),
	                 [](const FSceneChange& InA, const FSceneChange& InB)
	                 {
		                 return InA.Revision < InB.Revision;
	                 });
	return Result;
}

void FScene::Acknowledge(std::uint64_t InRevision)
{
	RequireMain();
	std::erase_if(Storage->Changes,
	              [InRevision](const auto& InPair)
	              {
		              return InPair.second.Revision <= InRevision;
	              });
}

void FScene::Clear()
{
	RequireMain();
	if (Storage->Order.empty())
	{
		return;
	}
	FSceneStorage::FMutation Mutation(*Storage);
	for (const auto Slot : Storage->Order)
	{
		Mutation.Remove(Slot);
	}
	Mutation.Roots = std::vector<std::uint32_t>{};
	Mutation.Commit();
}

void FScene::BeginSynchronization()
{
	RequireMain();
	if (Storage->bSynchronizing)
	{
		throw std::logic_error("Logical scene already has a synchronization consumer");
	}
	FSceneStorage::FMutation Mutation(*Storage);
	for (const auto Slot : Storage->Order)
	{
		Mutation.Mark(Slot, ESceneChangeMask::Structure);
	}
	Mutation.Commit(false, true);
	Storage->bSynchronizing = true;
}

void FScene::EndSynchronization()
{
	RequireMain();
	Storage->bSynchronizing = false;
}

FSceneHandle FScene::Add(FSceneModel InModel)
{
	RequireMain();
	FSceneNode Node;
	Node.Name = std::move(InModel.Name);
	Node.Local = InModel.World;
	Node.Model = SceneModelComponent(InModel);
	return AddNode(std::move(Node));
}

bool FScene::Update(FSceneHandle InHandle, FSceneModel InModel)
{
	const auto* Existing = FindNode(InHandle);
	if (!Existing || !Existing->Model)
	{
		return false;
	}
	auto Node = *Existing;
	Node.Name = std::move(InModel.Name);
	const auto& Entry = *Storage->Slots[InHandle.Slot];
	if (Entry.World.Values != InModel.World.Values)
	{
		Node.Local = Storage->ToLocal(Entry.Parent, InModel.World);
	}
	Node.Model = SceneModelComponent(InModel);
	// Find exposes effective visibility. An unchanged transfer bit must not author inherited hiding.
	if (InModel.bVisible == Entry.Transfer.bVisible)
	{
		Node.Model->bVisible = Existing->Model->bVisible;
	}
	if (Existing->Model->Data == Node.Model->Data)
	{
		Node.Model->Asset = Existing->Model->Asset;
	}
	return Storage->EditNode(InHandle, std::move(Node));
}

bool FScene::Remove(FSceneHandle InHandle)
{
	const auto* Existing = FindNode(InHandle);
	return Existing && Existing->Model ? RemoveNodeKeepChildren(InHandle) : false;
}

const FSceneModel* FScene::Find(FSceneHandle InHandle) const
{
	RequireMain();
	const auto Slot = Storage->Live(InHandle);
	return Slot == FSceneStorage::InvalidSlot || !Storage->Slots[Slot]->Node->Model ? nullptr
	                                                                                : &Storage->Slots[Slot]->Transfer;
}

std::vector<FSceneHandle> FScene::GetHandles() const
{
	return GetNodes(ESceneNodeKind::Model);
}
} // namespace Hyperion
