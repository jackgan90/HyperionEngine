#pragma once
#include "Hyperion/RasterOptions/Viewport.h"
#include "Hyperion/Reflection/RecordValue.h"
#include "Hyperion/Scene/LightShadows.h"

namespace Hyperion
{
struct FCascadedShadowSettings : FDirectionalShadowSettings
{
	std::optional<FViewport> PreviewViewport; // Host may reserve space beside its UI; pixel coordinates.
};

class IShadowControls
{
public:
	virtual ~IShadowControls() = default;
	virtual FCascadedShadowSettings ShadowControls() const = 0;
	virtual void SetShadowControls(const FCascadedShadowSettings& InSettings) = 0;
};

void ValidateShadowSettings(const FCascadedShadowSettings& InSettings);
template<> const FRecordDescriptor& RecordType<FCascadedShadowSettings>();
} // namespace Hyperion
