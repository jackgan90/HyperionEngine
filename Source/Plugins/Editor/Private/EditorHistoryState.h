#pragma once
#include "EditorSelection.h"

namespace Hyperion
{
struct FEditorNodeHistory
{
	FSceneHandle Handle;
	FSceneNode Before;
	FSceneNode After;
};

struct FEditorHistoryEntry
{
	FSceneHandle Handle;
	std::optional<FSceneNode> Before;
	std::optional<FSceneNode> After;
	FSceneSettings BeforeSettings;
	FSceneSettings AfterSettings;
	std::uint64_t BeforeState{};
	std::uint64_t AfterState{};
	std::vector<std::pair<FSceneHandle, FSceneNode>> DeletedSubtree;
	std::vector<FEditorNodeHistory> Edits;
	std::vector<FSceneHandle> DeletedRoots;
	FEditorSelection BeforeSelection;
};

inline void RemapEditorHandle(FSceneHandle& InHandle, const FEditorHandleMap& InMapping)
{
	if (const auto Found = InMapping.find(InHandle); Found != InMapping.end())
	{
		InHandle = Found->second;
	}
}

inline void RemapEditorHandle(std::optional<FSceneHandle>& InHandle, const FEditorHandleMap& InMapping)
{
	if (InHandle)
	{
		RemapEditorHandle(*InHandle, InMapping);
	}
}

// A complete mapping is applied simultaneously, including handles referenced by future redo entries.
inline void RemapEditorHistory(std::span<FEditorHistoryEntry> InHistory, const FEditorHandleMap& InMapping)
{
	for (auto& Entry : InHistory)
	{
		Entry.BeforeSelection.Remap(InMapping);
		for (auto& Handle : Entry.DeletedRoots)
		{
			RemapEditorHandle(Handle, InMapping);
		}
		for (auto& Edit : Entry.Edits)
		{
			RemapEditorHandle(Edit.Handle, InMapping);
		}
		for (auto& [Handle, Node] : Entry.DeletedSubtree)
		{
			RemapEditorHandle(Handle, InMapping);
		}
		RemapEditorHandle(Entry.Handle, InMapping);
		for (auto* Settings : {&Entry.BeforeSettings, &Entry.AfterSettings})
		{
			for (auto* Reference :
			     {&Settings->DefaultCamera, &Settings->MainDirectionalLight, &Settings->EnvironmentLight})
			{
				RemapEditorHandle(*Reference, InMapping);
			}
		}
	}
}
} // namespace Hyperion
