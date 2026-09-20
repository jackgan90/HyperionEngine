#pragma once
#include "Hyperion/Scene/Scene.h"
#include <limits>
#include <set>
#include <thread>

namespace Hyperion
{
class FSceneStorage
{
public:
	static constexpr std::uint32_t InvalidSlot = std::numeric_limits<std::uint32_t>::max();

	struct FSlot
	{
		std::uint64_t Generation{};
		std::optional<FSceneNode> Node;
		std::uint32_t Parent = InvalidSlot;
		std::vector<std::uint32_t> Children;
		FMat4 World = Hyperion::Identity();
		bool bEffectiveEnabled{};
		FSceneModel Transfer;
	};

	struct FMutation
	{
		explicit FMutation(FSceneStorage& InStorage);
		FSceneStorage& Storage;
		std::map<std::uint32_t, std::unique_ptr<FSlot>> Slots;
		std::map<std::uint32_t, ESceneChangeMask> Masks;
		std::optional<std::vector<std::uint32_t>> Roots;
		std::optional<std::vector<std::uint32_t>> Order;
		std::optional<std::vector<std::uint32_t>> Free;
		std::map<std::string, std::uint32_t, std::less<>> AddedIds;
		std::vector<std::string> RemovedIds;
		FSceneSettings Settings;
		std::uint64_t NextSerial{};
		std::uint32_t SlotCount{};
		FSlot& Edit(std::uint32_t InSlot);
		const FSlot& Read(std::uint32_t InSlot) const;
		void Mark(std::uint32_t InSlot, ESceneChangeMask InMask);
		void Link(std::uint32_t InSlot, std::uint32_t InParent);
		void Unlink(std::uint32_t InSlot);
		std::uint32_t Add(FSceneNode InNode, std::uint32_t InParent);
		void Derive(std::uint32_t InRoot);
		void Remove(std::uint32_t InSlot);
		void StageNode(std::uint32_t InSlot, FSceneNode InNode);
		void ValidateHierarchy(std::span<const std::uint32_t> InSlots) const;
		void DeriveRoots(std::span<const std::uint32_t> InSlots);
		std::map<FSceneHandle, FSceneChange> PrepareChanges(std::uint64_t InRevision, bool bInSettingsChanged);
		void Commit(bool bInAdvanceRevision = true, bool bInForceSettings = false);
	};

	std::thread::id Owner = std::this_thread::get_id();
	std::uint64_t Identity{};
	std::uint64_t Revision{};
	std::uint64_t NextSerial = 1;
	std::vector<std::unique_ptr<FSlot>> Slots;
	std::vector<std::uint32_t> Roots;
	std::vector<std::uint32_t> Order;
	std::vector<std::uint32_t> Free;
	std::map<std::string, std::uint32_t, std::less<>> Ids;
	std::map<FSceneHandle, FSceneChange> Changes;
	FSceneSettings Settings;
	std::array<std::size_t, 7> Counts{};
	bool bSynchronizing{};
	std::unique_ptr<IBoundsSpatialIndex> QueryIndex;
	std::set<std::uint32_t> QueryDirty;

	std::uint32_t Live(FSceneHandle InHandle) const;
	std::uint32_t FindId(std::string_view InId) const;
	FSceneHandle Handle(std::uint32_t InSlot) const;
	std::vector<FSceneHandle> Handles(const std::vector<std::uint32_t>& InSlots) const;
	std::vector<std::uint32_t> Subtree(std::uint32_t InRoot) const;
	FSceneHandle AddNode(FSceneNode InNode);
	std::vector<FSceneHandle> LoadNodes(std::vector<FSceneNode> InNodes);
	bool EditNode(FSceneHandle InHandle, FSceneNode InNode);
	bool EditNodes(std::vector<FSceneNodeEdit> InEdits);
	std::vector<FSceneHandle> AddNodes(std::vector<FSceneNode> InNodes);
	bool RemoveSubtrees(std::span<const FSceneHandle> InHandles);
	bool Reparent(FSceneHandle InHandle, std::optional<FSceneHandle> InParent, ESceneReparentMode InMode);
	bool RemoveNodes(FSceneHandle InHandle, bool bInKeepChildren);
	FMat4 ToLocal(std::uint32_t InParent, const FMat4& InWorld) const;
	void ValidateSettings(const FSceneSettings& InSettings) const;
};

void ValidateSceneWorld(const FSceneNode& InNode, const FMat4& InWorld);
} // namespace Hyperion
