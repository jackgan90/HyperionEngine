#pragma once
#include "Hyperion/Scene/SceneNode.h"
#include <algorithm>

namespace Hyperion
{
// Selection order is explicit editor state; the last remaining object owns the gizmo.
class FEditorSelection
{
public:
	FEditorSelection() = default;

	FEditorSelection(std::optional<FSceneHandle> InHandle)
	{
		*this = InHandle;
	}

	FEditorSelection& operator=(std::optional<FSceneHandle> InHandle)
	{
		Objects.clear();
		if (InHandle)
		{
			Objects.push_back(*InHandle);
		}
		return *this;
	}

	explicit operator bool() const
	{
		return !Objects.empty();
	}

	FSceneHandle operator*() const
	{
		return Objects.back();
	}

	bool operator==(const FEditorSelection&) const = default;

	bool operator==(std::optional<FSceneHandle> InHandle) const
	{
		return Primary() == InHandle;
	}

	std::optional<FSceneHandle> Primary() const
	{
		return Objects.empty() ? std::nullopt : std::optional{Objects.back()};
	}

	const std::vector<FSceneHandle>& All() const
	{
		return Objects;
	}

	bool Contains(FSceneHandle InHandle) const
	{
		return std::find(Objects.begin(), Objects.end(), InHandle) != Objects.end();
	}

	void Toggle(FSceneHandle InHandle)
	{
		if (Contains(InHandle))
		{
			std::erase(Objects, InHandle);
		}
		else
		{
			Objects.push_back(InHandle);
		}
	}

	void Remap(FSceneHandle InBefore, FSceneHandle InAfter)
	{
		std::replace(Objects.begin(), Objects.end(), InBefore, InAfter);
	}

	void Clear()
	{
		Objects.clear();
	}

private:
	std::vector<FSceneHandle> Objects;
};
} // namespace Hyperion
