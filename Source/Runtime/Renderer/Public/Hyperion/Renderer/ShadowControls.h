#pragma once
#include "Hyperion/Reflection/RecordValue.h"
#include "Hyperion/Renderer/CascadedShadowMap.h"

namespace Hyperion
{
class IShadowControls
{
public:
	virtual ~IShadowControls() = default;
	virtual FCascadedShadowSettings ShadowControls() const = 0;
	virtual void SetShadowControls(const FCascadedShadowSettings& InSettings) = 0;
};

void ValidateShadowSettings(const FCascadedShadowSettings& InSettings);
// Applies only authored values; otherwise leaves caller-owned legacy defaults intact.
void ResolveSceneLightShadows(const FSceneMetadata* InMetadata, FCascadedShadowSettings& InOutDirectional,
                              FContactShadowSettings& InOutContact);
template<> const FRecordDescriptor& RecordType<FCascadedShadowSettings>();
} // namespace Hyperion
