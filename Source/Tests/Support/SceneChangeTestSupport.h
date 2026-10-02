#pragma once
#include "Hyperion/Scene/Scene.h"
#include <source_location>

namespace Hyperion
{
struct FSceneFactTestValue
{
	int Value{};
	bool operator==(const FSceneFactTestValue&) const = default;
};

struct FSceneFactOtherValue
{
	int Value{};
	bool operator==(const FSceneFactOtherValue&) const = default;
};

template<> inline const FRecordDescriptor& RecordType<FSceneFactTestValue>()
{
	static const auto Type =
	    MakeRecord<FSceneFactTestValue>("test.scene-fact", {Member("value", &FSceneFactTestValue::Value)});
	return Type;
}

template<> inline const FRecordDescriptor& RecordType<FSceneFactOtherValue>()
{
	static const auto Type =
	    MakeRecord<FSceneFactOtherValue>("test.scene-fact-other", {Member("value", &FSceneFactOtherValue::Value)});
	return Type;
}

namespace Tests
{
inline void CheckSceneFact(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error(std::string("Scene fact check failed: ") + InLocation.file_name() + ":" +
		                         std::to_string(InLocation.line()));
	}
}

struct FSceneFactFault
{
	const std::any* GetState{};
	std::optional<int> EqualValue;
	std::size_t EqualCalls{};
	std::size_t GetFailures{};
};

inline thread_local FSceneFactFault SceneFactFault;

inline void RegisterSceneFactTypes()
{
	if (SceneComponentRegistry().TryFind("test.scene-fact"))
	{
		return;
	}
	auto Type = MakeSceneComponent<FSceneFactTestValue>("Scene fact test");
	Type.bUnique = false;
	const auto Get = Type.Get;
	Type.Get = [Get](const std::any& InState)
	{
		if (SceneFactFault.GetState == &InState)
		{
			++SceneFactFault.GetFailures;
			throw std::runtime_error("Scene fact Get failure");
		}
		return Get(InState);
	};
	const auto Equal = Type.Equal;
	Type.Equal = [Equal](const std::any& InBefore, const std::any& InAfter)
	{
		++SceneFactFault.EqualCalls;
		const auto& Value = std::any_cast<const std::optional<FSceneFactTestValue>&>(InBefore);
		if (SceneFactFault.EqualValue && Value && Value->Value == *SceneFactFault.EqualValue)
		{
			throw std::runtime_error("Scene fact Equal failure");
		}
		return Equal(InBefore, InAfter);
	};
	SceneComponentRegistry().Register(std::move(Type));
	SceneComponentRegistry().Register(MakeSceneComponent<FSceneFactOtherValue>("Other scene fact test"));
}

inline void AddSceneFactValue(FSceneNode& InNode, std::string InId, int InValue = 0)
{
	InNode.Components.Add(InId, "test.scene-fact");
	static_cast<FSceneFactTestValue*>(InNode.Components.Find(InId)->Edit())->Value = InValue;
}

inline void SetSceneFactValue(FSceneNode& InNode, std::string_view InId, int InValue)
{
	static_cast<FSceneFactTestValue*>(InNode.Components.Find(InId)->Edit())->Value = InValue;
}

inline FSceneChange SceneFactChange(const FScene& InScene, FSceneHandle InHandle)
{
	for (const auto& Change : InScene.GetChanges())
	{
		if (Change.Handle == InHandle)
		{
			return Change;
		}
	}
	throw std::runtime_error("Missing expected scene change");
}

inline void CheckSceneFacts(const FScene& InScene, FSceneHandle InHandle,
                            std::initializer_list<FSceneComponentChange> InExpected,
                            std::source_location InLocation = std::source_location::current())
{
	CheckSceneFact(SceneFactChange(InScene, InHandle).ComponentChanges ==
	                   std::vector<FSceneComponentChange>(InExpected),
	               InLocation);
}

struct FSceneFactNodeSnapshot
{
	FSceneHandle Handle;
	FSceneNode Node;
	std::vector<FSceneHandle> Children;
	FMat4 World;
	bool bEffectiveEnabled{};
};

struct FSceneFactSnapshot
{
	std::uint64_t Revision{};
	std::vector<FSceneFactNodeSnapshot> Nodes;
	std::vector<FSceneHandle> Roots;
	std::array<std::size_t, 7> Counts{};
	FSceneSettings Settings;
	std::vector<FSceneChange> Changes;
	FSceneRayResult Ray;
};

inline FRay SceneFactRay()
{
	return {{0, 0, 5}, {0, 0, -1}, 0, 20};
}

inline FSceneHandle AddSceneFactQueryModel(FScene& InScene)
{
	auto Asset = std::make_shared<FModelAsset>();
	Asset->MaterialSlots = {{"", "Test.hasset", "hyperion.materialasset", ""}};
	FModelPrimitive Primitive;
	Primitive.Id = "triangle";
	Primitive.Material = 0;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 0, 1, 0};
	Primitive.Indices = {0, 1, 2};
	Asset->Primitives.push_back(Primitive);
	FModelNode Node;
	Node.Id = "mesh";
	Node.Primitives = {0};
	Asset->Nodes.push_back(Node);
	Asset->Roots = {0};
	auto Data = std::make_shared<FSceneModelData>(*PrepareSceneModel(Asset));
	Data->QueryGeometry = PrepareSceneModelGeometry(*Asset);
	FSceneModel Model;
	Model.Name = "query model";
	Model.Data = std::move(Data);
	return InScene.Add(std::move(Model));
}

inline FSceneFactSnapshot SnapshotSceneFacts(FScene& InScene)
{
	FSceneFactSnapshot Result;
	Result.Revision = InScene.GetRevision();
	Result.Roots = InScene.GetRoots();
	Result.Settings = InScene.GetSettings();
	Result.Changes = InScene.GetChanges();
	for (const auto Handle : InScene.GetNodes())
	{
		FMat4 World;
		CheckSceneFact(InScene.GetWorld(Handle, World));
		Result.Nodes.push_back({Handle, *InScene.FindNode(Handle), InScene.GetChildren(Handle), World,
		                        InScene.IsEffectivelyEnabled(Handle)});
	}
	for (std::size_t Index = 0; Index < Result.Counts.size(); ++Index)
	{
		Result.Counts[Index] = InScene.CountNodes(static_cast<ESceneNodeKind>(Index));
	}
	InScene.Raycast(SceneFactRay());
	Result.Ray = InScene.Raycast(SceneFactRay());
	CheckSceneFact(Result.Ray.Stats.Bounds.IndexRebuilds == 0 && Result.Ray.Stats.Bounds.IndexRefits == 0);
	return Result;
}

inline void CheckSceneFactModel(const std::optional<FSceneModel>& InBefore, const std::optional<FSceneModel>& InAfter)
{
	CheckSceneFact(InBefore.has_value() == InAfter.has_value());
	if (InBefore)
	{
		CheckSceneFact(InBefore->Name == InAfter->Name && InBefore->Data == InAfter->Data &&
		               InBefore->World.Values == InAfter->World.Values && InBefore->bVisible == InAfter->bVisible &&
		               InBefore->Material == InAfter->Material && InBefore->Surface == InAfter->Surface &&
		               InBefore->SectionSurfaces == InAfter->SectionSurfaces &&
		               InBefore->SourceNode == InAfter->SourceNode && InBefore->Sections == InAfter->Sections &&
		               InBefore->SourcePrimitive == InAfter->SourcePrimitive);
	}
}

inline void CheckSceneFactSnapshot(FScene& InScene, const FSceneFactSnapshot& InBefore)
{
	const auto Ray = InScene.Raycast(SceneFactRay());
	const auto After = SnapshotSceneFacts(InScene);
	CheckSceneFact(After.Revision == InBefore.Revision && After.Roots == InBefore.Roots &&
	               After.Settings == InBefore.Settings && After.Counts == InBefore.Counts &&
	               After.Nodes.size() == InBefore.Nodes.size() && After.Changes.size() == InBefore.Changes.size());
	for (std::size_t Index = 0; Index < After.Nodes.size(); ++Index)
	{
		const auto& Old = InBefore.Nodes[Index];
		const auto& Current = After.Nodes[Index];
		CheckSceneFact(Current.Handle == Old.Handle && Current.Node == Old.Node && Current.Children == Old.Children &&
		               Current.World.Values == Old.World.Values && Current.bEffectiveEnabled == Old.bEffectiveEnabled);
	}
	for (std::size_t Index = 0; Index < After.Changes.size(); ++Index)
	{
		const auto& Old = InBefore.Changes[Index];
		const auto& Current = After.Changes[Index];
		CheckSceneFact(Current.Handle == Old.Handle && Current.Revision == Old.Revision && Current.Mask == Old.Mask &&
		               Current.bRemoved == Old.bRemoved && Current.Kind == Old.Kind && Current.Node == Old.Node &&
		               Current.World.Values == Old.World.Values && Current.bEffectiveEnabled == Old.bEffectiveEnabled &&
		               Current.Settings == Old.Settings && Current.ComponentChanges == Old.ComponentChanges);
		CheckSceneFactModel(Old.Model, Current.Model);
	}
	CheckSceneFact(Ray.Status == InBefore.Ray.Status && Ray.Handle == InBefore.Ray.Handle &&
	               Ray.Distance == InBefore.Ray.Distance && Ray.Instance == InBefore.Ray.Instance &&
	               Ray.Primitive == InBefore.Ray.Primitive && Ray.Triangle == InBefore.Ray.Triangle &&
	               Ray.bIncomplete == InBefore.Ray.bIncomplete && Ray.Stats.Bounds.IndexRebuilds == 0 &&
	               Ray.Stats.Bounds.IndexRefits == 0);
}
} // namespace Tests
} // namespace Hyperion
