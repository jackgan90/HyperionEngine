#pragma once
#include "Hyperion/Scene/Model.h"
#include <string_view>

namespace Hyperion
{
struct FSceneAssetEntry
{
	std::string Id;
	std::string Path;
};

struct FSceneInstanceEntry
{
	std::string Id;
	std::string Asset;
	FVec3 Translation;
	FVec4 Rotation{0, 0, 0, 1};
	FVec3 Scale{1, 1, 1};
	bool bVisible = true;
};

struct FSceneManifest
{
	std::vector<FSceneAssetEntry> Assets;
	std::vector<FSceneInstanceEntry> Instances;
	FVec3 Eye{0, 3, 12};
	FVec3 Target;
	float Near = .01f;
	float Far = 1000;
};

void ValidateSceneManifest(const FSceneManifest& InManifest);
FSceneManifest DecodeSceneManifest(std::string_view InText);
template<> const FRecordDescriptor& RecordType<FSceneAssetEntry>();
template<> const FRecordDescriptor& RecordType<FSceneInstanceEntry>();
template<> const FRecordDescriptor& RecordType<FSceneManifest>();
} // namespace Hyperion
