#pragma once
#include "Hyperion/Scene/SceneNode.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Hyperion
{
struct FEditorHandleHash
{
	std::size_t operator()(FSceneHandle InHandle) const noexcept
	{
		auto Hash = std::hash<std::uint64_t>{}(InHandle.Scene);
		for (const auto Value : {std::uint64_t(InHandle.Slot), InHandle.Generation})
		{
			Hash ^= std::hash<std::uint64_t>{}(Value) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		}
		return Hash;
	}
};

using FEditorHandleMap = std::unordered_map<FSceneHandle, FSceneHandle, FEditorHandleHash>;

// Selection order is explicit editor state; the last remaining object owns the gizmo.
class FEditorSelection
{
public:
	FEditorSelection() = default;
	FEditorSelection(const FEditorSelection&) = default;
	FEditorSelection(FEditorSelection&&) noexcept = default;
	FEditorSelection& operator=(FEditorSelection&&) noexcept = default;

	FEditorSelection& operator=(const FEditorSelection& InOther)
	{
		FEditorSelection Updated(InOther);
		Swap(Updated);
		return *this;
	}

	FEditorSelection(std::optional<FSceneHandle> InHandle)
	{
		*this = InHandle;
	}

	FEditorSelection& operator=(std::optional<FSceneHandle> InHandle)
	{
		FEditorSelection Updated;
		if (InHandle)
		{
			Updated.Toggle(*InHandle);
		}
		Swap(Updated);
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

	bool operator==(const FEditorSelection& InOther) const
	{
		return Objects == InOther.Objects;
	}

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
		return Membership.contains(InHandle);
	}

	void Toggle(FSceneHandle InHandle)
	{
		if (Contains(InHandle))
		{
			std::erase(Objects, InHandle);
			Membership.erase(InHandle);
		}
		else
		{
			Objects.push_back(InHandle);
			try
			{
				Membership.insert(InHandle);
			}
			catch (...)
			{
				Objects.pop_back();
				throw;
			}
		}
	}

	void Remap(FSceneHandle InBefore, FSceneHandle InAfter)
	{
		Remap(FEditorHandleMap{{InBefore, InAfter}});
	}

	void Remap(const FEditorHandleMap& InMapping)
	{
		FEditorSelection Updated;
		Updated.Objects.reserve(Objects.size());
		Updated.Membership.reserve(Objects.size());
		for (const auto Handle : Objects)
		{
			const auto Found = InMapping.find(Handle);
			const auto Remapped = Found == InMapping.end() ? Handle : Found->second;
			if (!Updated.Contains(Remapped))
			{
				Updated.Toggle(Remapped);
			}
		}
		Swap(Updated);
	}

	void Clear()
	{
		Objects.clear();
		Membership.clear();
	}

private:
	void Swap(FEditorSelection& InOther) noexcept
	{
		Objects.swap(InOther.Objects);
		Membership.swap(InOther.Membership);
	}

	std::vector<FSceneHandle> Objects;
	std::unordered_set<FSceneHandle, FEditorHandleHash> Membership;
};
} // namespace Hyperion
