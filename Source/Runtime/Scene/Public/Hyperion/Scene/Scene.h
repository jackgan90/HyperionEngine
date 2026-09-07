#pragma once
#include "Hyperion/Materials/Material.h"
#include "Hyperion/Scene/Model.h"
#include <compare>
#include <optional>
#include <thread>

namespace Hyperion
{
struct FMaterialOverride
{
	std::optional<FVec4> BaseColor;
	std::optional<float> Metallic;
	std::optional<float> Roughness;
};

// Prepared once on a loader/Worker, shared by independently transformed instances.
struct FSceneModelData
{
	std::shared_ptr<const FModelAsset> Asset;
	std::vector<FModelInstance> Instances;
	std::vector<FBounds> PrimitiveBounds;
	FBounds Bounds;
};

std::shared_ptr<const FSceneModelData> PrepareSceneModel(std::shared_ptr<const FModelAsset> InAsset);
void ValidateMaterialOverride(const FMaterialOverride& InMaterial);

struct FSceneHandle
{
	std::uint64_t Scene{};
	std::uint32_t Slot{};
	std::uint64_t Generation{};
	auto operator<=>(const FSceneHandle&) const = default;
};

struct FSceneMaterialSelection
{
	// Choose either a Main-owned editable instance or an already frozen snapshot; empty means inherited.
	std::shared_ptr<FMaterialInstance> Instance;
	std::shared_ptr<const FMaterialSnapshot> Snapshot;
	FMaterialParameterValues Overrides;
};

struct FSceneModel
{
	std::string Name;
	std::shared_ptr<const FSceneModelData> Data;
	FMat4 World = Identity();
	bool bVisible = true;
	FMaterialOverride Material;
	FSceneMaterialSelection Surface;
	std::map<std::uint32_t, FSceneMaterialSelection> SectionSurfaces;
};

void ValidateSceneMaterialSelections(const FSceneModel& InModel);

struct FSceneChange
{
	FSceneHandle Handle;
	std::uint64_t Revision{};
	std::optional<FSceneModel> Model;
};

// Single Main owner; one bridge consumes/acknowledges changes. Returned pointers are Main-only.
class FScene
{
public:
	FScene();
	FScene(const FScene&) = delete;
	FScene& operator=(const FScene&) = delete;
	void RequireMain() const;
	std::uint64_t GetIdentity() const;
	FSceneHandle Add(FSceneModel InModel);
	bool Update(FSceneHandle InHandle, FSceneModel InModel);
	bool Remove(FSceneHandle InHandle);
	const FSceneModel* Find(FSceneHandle InHandle) const;
	std::vector<FSceneHandle> GetHandles() const;
	std::vector<FSceneChange> GetChanges() const;
	void Acknowledge(std::uint64_t InRevision);
	void Clear();
	void BeginSynchronization();
	void EndSynchronization();

private:
	struct FSlot
	{
		std::uint64_t Generation{};
		std::optional<FSceneModel> Model;
	};

	std::thread::id Owner;
	std::uint64_t Identity{};
	std::uint64_t Revision{};
	std::vector<FSlot> Slots;
	std::map<FSceneHandle, FSceneChange> Changes;
	bool bSynchronizing{};
};
} // namespace Hyperion
