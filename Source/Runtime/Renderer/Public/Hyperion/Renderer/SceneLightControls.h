#pragma once
#include "Hyperion/Scene/Scene.h"

namespace Hyperion
{
enum class ESceneSkyState
{
	None,
	Unrequested,
	Loading,
	Uploading,
	Ready,
	Failed
};

struct FSceneSkyStatus
{
	ESceneSkyState State = ESceneSkyState::None;
	std::string Error;
};

std::string_view SceneSkyStateName(ESceneSkyState InState);
std::string FormatSceneSkyStatus(const FSceneSkyStatus& InStatus);
template<> std::span<const ESceneSkyState> RecordEnumValues<ESceneSkyState>();

struct FSceneLightDiagnostic
{
	FSceneHandle Handle;
	std::string Id;
	std::string Type;
	std::int32_t Priority{};
	bool bEnabled{};
	bool bSelected{};
	bool bTied{};
	std::string Message;
	FSceneSkyStatus Asset;
};

struct FSceneLightingInfo
{
	std::uint64_t Revision{};
	std::optional<FSceneHandle> ShadowDirectionalLight;
	std::optional<FSceneHandle> SkyLight;
	std::vector<FSceneLightDiagnostic> Lights;
};

class ISceneLightControls
{
public:
	virtual ~ISceneLightControls() = default;
	virtual FSceneLightingInfo LightingInfo() = 0;
};

template<> const FRecordDescriptor& RecordType<FSceneSkyStatus>();
template<> const FRecordDescriptor& RecordType<FSceneLightDiagnostic>();
template<> const FRecordDescriptor& RecordType<FSceneLightingInfo>();
} // namespace Hyperion
