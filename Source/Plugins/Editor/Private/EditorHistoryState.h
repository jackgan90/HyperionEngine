#pragma once
#include "EditorSelection.h"
#include "Hyperion/SceneEditing/SceneHistory.h"

namespace Hyperion
{
using FEditorNodeHistory = FSceneNodeHistory;
using FEditorHistoryEntry = FSceneHistoryEntry;

inline void RemapEditorHandle(std::optional<FSceneHandle>& InHandle, const FEditorHandleMap& InMapping)
{
	RemapSceneHandle(InHandle, InMapping);
}

inline void RemapEditorHistory(std::span<FEditorHistoryEntry> InHistory, const FEditorHandleMap& InMapping)
{
	RemapSceneHistory(InHistory, InMapping);
}
} // namespace Hyperion
