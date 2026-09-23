#pragma once
#include "Hyperion/Scene/Scene.h"

namespace Hyperion
{
struct FSceneMainLight
{
	std::uint64_t Revision{};
	FSceneHandle Handle;
	FSceneDirectionalLight Light;
	FVec3 Direction{0, 1, 0};
};

class ISceneLightControls
{
public:
	virtual ~ISceneLightControls() = default;
	virtual FSceneMainLight MainLight() = 0;
	virtual void SetMainLight(const FSceneMainLight& InLight) = 0;
};

template<> const FRecordDescriptor& RecordType<FSceneMainLight>();
} // namespace Hyperion
