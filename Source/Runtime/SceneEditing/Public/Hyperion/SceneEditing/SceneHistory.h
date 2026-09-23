#pragma once
#include "Hyperion/SceneEditing/SceneSelection.h"

namespace Hyperion
{
struct FSceneNodeHistory
{
	FSceneHandle Handle;
	FSceneNode Before;
	FSceneNode After;
};

struct FSceneHistoryEntry
{
	FSceneHandle Handle;
	std::optional<FSceneNode> Before;
	std::optional<FSceneNode> After;
	FSceneSettings BeforeSettings;
	FSceneSettings AfterSettings;
	std::uint64_t BeforeState{};
	std::uint64_t AfterState{};
	std::vector<std::pair<FSceneHandle, FSceneNode>> DeletedSubtree;
	std::vector<FSceneNodeHistory> Edits;
	std::vector<FSceneHandle> DeletedRoots;
	FSceneSelection BeforeSelection;
};

inline void RemapSceneHandle(FSceneHandle& InHandle, const FSceneHandleMap& InMapping)
{
	if (const auto Found = InMapping.find(InHandle); Found != InMapping.end())
	{
		InHandle = Found->second;
	}
}

inline void RemapSceneHandle(std::optional<FSceneHandle>& InHandle, const FSceneHandleMap& InMapping)
{
	if (InHandle)
	{
		RemapSceneHandle(*InHandle, InMapping);
	}
}

// A complete mapping is applied simultaneously, including handles referenced by future redo entries.
inline void RemapSceneHistory(std::span<FSceneHistoryEntry> InHistory, const FSceneHandleMap& InMapping)
{
	for (auto& Entry : InHistory)
	{
		Entry.BeforeSelection.Remap(InMapping);
		for (auto& Handle : Entry.DeletedRoots)
		{
			RemapSceneHandle(Handle, InMapping);
		}
		for (auto& Edit : Entry.Edits)
		{
			RemapSceneHandle(Edit.Handle, InMapping);
		}
		for (auto& [Handle, Node] : Entry.DeletedSubtree)
		{
			RemapSceneHandle(Handle, InMapping);
		}
		RemapSceneHandle(Entry.Handle, InMapping);
		for (auto* Settings : {&Entry.BeforeSettings, &Entry.AfterSettings})
		{
			for (auto* Reference :
			     {&Settings->DefaultCamera, &Settings->MainDirectionalLight, &Settings->EnvironmentLight})
			{
				RemapSceneHandle(*Reference, InMapping);
			}
		}
	}
}
} // namespace Hyperion
