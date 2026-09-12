#pragma once
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
struct FSceneCamera
{
	float VerticalRadians = .729224966f;
	float Near = .01f;
	float Far = 1000.f;
	float FocusDistance = 12.f;
	bool operator==(const FSceneCamera&) const = default;
};

struct FSceneCameraPose
{
	FVec3 Eye;
	FVec3 Forward{0, 0, -1};
	FVec3 Up{0, 1, 0};
	FVec3 Right{1, 0, 0};
};

void ValidateSceneCamera(const FSceneCamera& InCamera);
bool TryExtractScenePose(const FMat4& InWorld, FSceneCameraPose& OutPose);
FSceneCameraPose ExtractScenePose(const FMat4& InWorld);
FMat4 SceneCameraTransform(FVec3 InEye, FVec3 InTarget, FVec3 InUp = {0, 1, 0});
template<> const FRecordDescriptor& RecordType<FSceneCamera>();
} // namespace Hyperion
