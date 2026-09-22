#pragma once
#include "Hyperion/Gui/Gui.h"

namespace Hyperion
{
class FAssetLiveEditScope
{
public:
	explicit FAssetLiveEditScope(FGui& InGui) : Gui(InGui)
	{
		Gui.BeginLiveEdit();
	}

	~FAssetLiveEditScope()
	{
		if (bActive)
		{
			Gui.EndLiveEdit();
		}
	}

	FGuiEditState Finish()
	{
		bActive = false;
		return Gui.EndLiveEdit();
	}

	FAssetLiveEditScope(const FAssetLiveEditScope&) = delete;
	FAssetLiveEditScope& operator=(const FAssetLiveEditScope&) = delete;

private:
	FGui& Gui;
	bool bActive = true;
};

inline bool AssetProperty(FGui& InGui, const char* InLabel, const std::function<bool()>& InWidget)
{
	InGui.BeginPropertyRow(InLabel);
	const bool bChanged = InWidget();
	InGui.EndPropertyRow();
	return bChanged;
}

inline bool AssetText(FGui& InGui, const char* InLabel, std::string& InValue, bool bInCommit = false)
{
	return AssetProperty(InGui, InLabel,
	                     [&]
	                     {
		                     return InGui.InputText("##value", InValue, bInCommit);
	                     });
}

inline bool AssetFloat(FGui& InGui, const char* InLabel, float& InValue)
{
	return AssetProperty(InGui, InLabel,
	                     [&]
	                     {
		                     return InGui.InputFloat("##value", InValue);
	                     });
}

inline bool AssetInteger(FGui& InGui, const char* InLabel, std::uint64_t& InValue)
{
	return AssetProperty(InGui, InLabel,
	                     [&]
	                     {
		                     return InGui.InputInteger("##value", InValue);
	                     });
}

inline bool AssetInteger(FGui& InGui, const char* InLabel, std::int64_t& InValue)
{
	return AssetProperty(InGui, InLabel,
	                     [&]
	                     {
		                     return InGui.InputInteger("##value", InValue);
	                     });
}

inline bool AssetCheckbox(FGui& InGui, const char* InLabel, bool& bInValue)
{
	return AssetProperty(InGui, InLabel,
	                     [&]
	                     {
		                     return InGui.Checkbox("##value", bInValue);
	                     });
}

inline bool AssetCombo(FGui& InGui, const char* InLabel, std::span<const std::string> InChoices, std::size_t& InIndex,
                       const std::function<void(std::size_t, FVec4)>& InObserve = {}, const char* InPreview = nullptr)
{
	return AssetProperty(InGui, InLabel,
	                     [&]
	                     {
		                     return InGui.Combo("##value", InChoices, InIndex, InObserve, InPreview);
	                     });
}

inline void AssetInfo(FGui& InGui, const char* InLabel, const std::string& InValue)
{
	InGui.BeginPropertyRow(InLabel);
	InGui.BeginDisabled(true);
	InGui.TextWrapped(InValue);
	InGui.EndDisabled();
	InGui.EndPropertyRow();
}
} // namespace Hyperion
