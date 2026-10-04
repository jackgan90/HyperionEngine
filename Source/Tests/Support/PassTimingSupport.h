#pragma once
#include "Hyperion/Renderer/RenderPassTiming.h"
#include "Support/TestSupport.h"

namespace Hyperion
{
// Independent migration oracle for built-in declarations; production never classifies by these labels.
inline void CheckBuiltinPassTiming(const FGpuFrameTiming& InFrame)
{

	struct FExpected
	{
		std::string_view Prefix;
		ERenderPassTimingCategory Category;
	};

	constexpr std::array Expected{FExpected{"Shadow cascade ", ERenderPassTimingCategory::Shadow},
	                              FExpected{"Forward/", ERenderPassTimingCategory::Forward},
	                              FExpected{"Deferred/BasePass/", ERenderPassTimingCategory::DeferredBase},
	                              FExpected{"Deferred/Lighting/", ERenderPassTimingCategory::Lighting},
	                              FExpected{"Deferred/LightingClustered/", ERenderPassTimingCategory::Lighting},
	                              FExpected{"Deferred/ClusterLighting/", ERenderPassTimingCategory::Lighting},
	                              FExpected{"Deferred/Compatibility/", ERenderPassTimingCategory::Compatibility},
	                              FExpected{"Scene/Transparent/", ERenderPassTimingCategory::Transparent},
	                              FExpected{"Output/Tonemap/", ERenderPassTimingCategory::Tonemap},
	                              FExpected{"Scene/Sky/", ERenderPassTimingCategory::Sky},
	                              FExpected{"HZB/", ERenderPassTimingCategory::HierarchicalDepth},
	                              FExpected{"Deferred/ContactShadowMask/", ERenderPassTimingCategory::ContactShadow},
	                              FExpected{"Deferred/LocalLights/", ERenderPassTimingCategory::LocalLights}};
	HYP_CHECK(!InFrame.Passes.empty());
	for (const auto& Pass : InFrame.Passes)
	{
		auto Category = ERenderPassTimingCategory::Unclassified;
		for (const auto& Entry : Expected)
		{
			if (Pass.Name.starts_with(Entry.Prefix))
			{
				Category = Entry.Category;
				break;
			}
		}
		const auto Timing = DecodeRenderPassTiming(Pass.Tag);
		HYP_CHECK((Timing ? Timing->Category : ERenderPassTimingCategory::Unclassified) == Category);
	}
}
} // namespace Hyperion
