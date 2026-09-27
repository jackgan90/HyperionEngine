#pragma once
#include "Hyperion/SceneEditing/SceneSelection.h"

namespace Hyperion
{
inline constexpr const char* SceneClipboardFormat = "Hyperion.SceneObjects.v1";

struct FSceneClipboardProvider
{
	std::function<std::string()> Read;
	std::function<void(const std::string&, const std::string&)> Write;
};

struct FSceneClipboardSnapshot
{
	std::string Token;
	std::string Document;
	std::vector<std::pair<FSceneHandle, FSceneNode>> Nodes;
	std::vector<FSceneHandle> Roots;
	FSceneSelection Selection;
	std::map<std::string, FSceneHandle, std::less<>> External;
	std::uint64_t AssetGeneration{};
	std::size_t Bytes{};
};

struct FSceneClipboardInfo
{
	bool bAvailable{};
	bool bCanPaste{};
	std::uint32_t Nodes{};
	std::uint32_t Selected{};
	std::string Reason;
};

template<> const FRecordDescriptor& RecordType<FSceneClipboardInfo>();
} // namespace Hyperion
