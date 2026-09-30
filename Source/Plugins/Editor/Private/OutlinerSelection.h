#pragma once
#include "EditorSelection.h"
#include <span>
#include <stdexcept>

namespace Hyperion
{
// Presentation order belongs to Outliner; the resulting set is committed by SceneEditing.
class FOutlinerSelectionState
{
public:
	void Reset()
	{
		Anchor.reset();
	}

	FEditorSelection Click(const FEditorSelection& InSelection, std::span<const FSceneHandle> InRows,
	                       FSceneHandle InEndpoint, bool bInRange, bool bInToggle)
	{
		const auto End = std::find(InRows.begin(), InRows.end(), InEndpoint);
		if (End == InRows.end())
		{
			throw std::invalid_argument("Selection endpoint is no longer an Outliner row");
		}
		if (!bInRange)
		{
			Anchor = InEndpoint;
			auto Updated = bInToggle ? InSelection : FEditorSelection{};
			Updated.Toggle(InEndpoint);
			return Updated;
		}
		auto Start = Anchor ? std::find(InRows.begin(), InRows.end(), *Anchor) : InRows.end();
		if (Start == InRows.end())
		{
			const auto Primary = InSelection.Primary();
			Start = Primary ? std::find(InRows.begin(), InRows.end(), *Primary) : InRows.end();
			if (Start == InRows.end())
			{
				Start = End;
			}
			Anchor = *Start;
		}
		auto Updated = bInToggle ? InSelection : FEditorSelection{};
		const auto [First, Last] = std::minmax(Start, End);
		for (auto Row = First; Row != Last + 1; ++Row)
		{
			if (!Updated.Contains(*Row))
			{
				Updated.Toggle(*Row);
			}
		}
		// The endpoint owns the gizmo even for reverse ranges or an already selected additive endpoint.
		Updated.Toggle(InEndpoint);
		Updated.Toggle(InEndpoint);
		return Updated;
	}

private:
	std::optional<FSceneHandle> Anchor;
};
} // namespace Hyperion
