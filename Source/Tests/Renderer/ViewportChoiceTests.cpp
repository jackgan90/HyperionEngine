#include "Hyperion/RasterOptions/RasterOptions.h"
#include "Hyperion/Renderer/ProfilingHud.h"
#include "Hyperion/Renderer/SceneViewport.h"
#include "Hyperion/Renderer/ViewportChoices.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <limits>

namespace
{
using namespace Hyperion;

template<class TAction> void Rejects(TAction InAction)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void FixedWireMeanings()
{
	HYP_CHECK(SceneCullingOptions().size() == 3 && OutlineOverlapOptions().size() == 2);
	HYP_CHECK(ParseSceneCullingMode(0) == ESceneCullingMode::None);
	HYP_CHECK(ParseSceneCullingMode(1) == ESceneCullingMode::Linear);
	HYP_CHECK(ParseSceneCullingMode(2) == ESceneCullingMode::Bvh);
	HYP_CHECK(ToCullingWireValue(ESceneCullingMode::None) == 0);
	HYP_CHECK(ToCullingWireValue(ESceneCullingMode::Linear) == 1);
	HYP_CHECK(ToCullingWireValue(ESceneCullingMode::Bvh) == 2);
	HYP_CHECK(ParseOutlineOverlapMode(0) == EOutlineOverlapMode::Union);
	HYP_CHECK(ParseOutlineOverlapMode(1) == EOutlineOverlapMode::PerObject);
	HYP_CHECK(ToOutlineWireValue(EOutlineOverlapMode::Union) == 0);
	HYP_CHECK(ToOutlineWireValue(EOutlineOverlapMode::PerObject) == 1);
	HYP_CHECK(RasterOptionLabels(SceneCullingOptions()) ==
	          (std::vector<std::string>{"None", "Linear frustum", "BVH frustum"}));
	HYP_CHECK(RasterOptionLabels(OutlineOverlapOptions()) == (std::vector<std::string>{"Union", "Per object"}));
}

void ProfilingCategories()
{
	const std::array Expected{EProfilingHudCategory::Overview,    EProfilingHudCategory::Tasks,
	                          EProfilingHudCategory::GpuPasses,   EProfilingHudCategory::DeviceCounters,
	                          EProfilingHudCategory::RenderViews, EProfilingHudCategory::LightingHzb,
	                          EProfilingHudCategory::Visibility,  EProfilingHudCategory::Batching};
	HYP_CHECK(ProfilingHudOptions().size() == Expected.size());
	for (std::size_t Index = 0; Index < Expected.size(); ++Index)
	{
		HYP_CHECK(ToProfilingHudWireValue(Expected[Index]) == (1u << Index));
	}
	std::vector<FProfilingHudOption> Presentation(ProfilingHudOptions().begin(), ProfilingHudOptions().end());
	std::reverse(Presentation.begin(), Presentation.end());
	Presentation.front().Label = "Changed presentation";
	auto Categories = EProfilingHudCategory::None;
	for (const auto& Option : Presentation)
	{
		Categories = WithProfilingHudCategory(Categories, Option.Id, true);
	}
	HYP_CHECK(ToProfilingHudWireValue(Categories) == 255);
	Categories = WithProfilingHudCategory(Categories, EProfilingHudCategory::Tasks, false);
	HYP_CHECK(ToProfilingHudWireValue(Categories) == 253);
	HYP_CHECK(!HasProfilingHudCategory(Categories, EProfilingHudCategory::Tasks));
	HYP_CHECK(HasProfilingHudCategory(Categories, EProfilingHudCategory::Batching));
	HYP_CHECK(ParseProfilingHudCategories(0) == EProfilingHudCategory::None);
	for (const auto Value : {256u, std::numeric_limits<std::uint32_t>::max()})
	{
		Rejects(
		    [&]
		    {
			    (void)ParseProfilingHudCategories(Value);
		    });
		Rejects(
		    [&]
		    {
			    (void)ToProfilingHudWireValue(static_cast<EProfilingHudCategory>(Value));
		    });
		FSceneViewportOptions Patch;
		Patch.ProfilingCategories = Value;
		Rejects(
		    [&]
		    {
			    ValidateViewportOptions(Patch, Patch);
		    });
		bool bUnavailable{};
		try
		{
			ValidateViewportOptions(Patch, {});
		}
		catch (const FSceneEditError& Error)
		{
			bUnavailable = Error.Code == SceneEditErrors::Unavailable;
		}
		HYP_CHECK(bUnavailable);
	}
}

template<class TOption, class TParse, class TEncode>
void Presentation(std::span<const TOption> InCanonical, TParse InParse, TEncode InEncode)
{
	std::vector<TOption> Options(InCanonical.begin(), InCanonical.end());
	std::reverse(Options.begin(), Options.end());
	Options.front().Label = "Relabeled choice";
	const std::span<const TOption> Reordered(Options);
	const auto Validate = [&]
	{
		ValidateViewportOptionPresentation(std::span<const TOption>(Options));
	};
	Validate();
	for (const auto& Option : InCanonical)
	{
		const auto Index = RasterOptionIndex(Reordered, InParse(Option.WireValue));
		HYP_CHECK(RasterOptionIdentity(Reordered, Index) == Option.Id);
		HYP_CHECK(InEncode(RasterOptionIdentity(Reordered, Index)) == Option.WireValue);
	}
	Rejects(
	    [&]
	    {
		    (void)RasterOptionIdentity(Reordered, Reordered.size());
	    });
	Options.front().WireValue = 99;
	Rejects(Validate);
	Options.assign(InCanonical.begin(), InCanonical.end());
	Options.front() = Options.back();
	Rejects(Validate);
	Options.assign(InCanonical.begin(), InCanonical.end());
	Options.front().Label = {};
	Rejects(Validate);
	Options.assign(InCanonical.begin(), InCanonical.end());
	Options.front().Id = static_cast<decltype(TOption::Id)>(99);
	Rejects(Validate);
	Options.assign(InCanonical.begin(), InCanonical.end());
	Options.pop_back();
	Rejects(Validate);
}

void InvalidChoices()
{
	for (const auto Value : {3u, 256u, std::numeric_limits<std::uint32_t>::max()})
	{
		HYP_CHECK(!IsSceneCullingWireValue(Value) && !IsOutlineOverlapWireValue(Value));
		Rejects(
		    [&]
		    {
			    (void)ParseSceneCullingMode(Value);
		    });
		Rejects(
		    [&]
		    {
			    (void)ParseOutlineOverlapMode(Value);
		    });
	}
	Rejects(
	    []
	    {
		    (void)ParseOutlineOverlapMode(2);
	    });
	Rejects(
	    []
	    {
		    (void)ToCullingWireValue(static_cast<ESceneCullingMode>(-1));
	    });
	Rejects(
	    []
	    {
		    (void)ToOutlineWireValue(static_cast<EOutlineOverlapMode>(255));
	    });
}

void ViewportPreflight()
{
	FSceneViewportOptions Supported;
	Supported.Culling = 2;
	Supported.OutlineMode = 0;
	Supported.Exposure = 1.f;
	ValidateViewportOptions({}, Supported);
	for (const auto Culling : {0u, 1u, 2u})
	{
		for (const auto Outline : {0u, 1u})
		{
			auto Patch = Supported;
			Patch.Culling = Culling;
			Patch.OutlineMode = Outline;
			ValidateViewportOptions(Patch, Supported);
		}
	}
	for (const auto Value : {3u, 256u, std::numeric_limits<std::uint32_t>::max()})
	{
		auto Patch = Supported;
		Patch.Exposure = 2.f;
		Patch.Culling = Value;
		Rejects(
		    [&]
		    {
			    ValidateViewportOptions(Patch, Supported);
		    });
		Patch.Culling.reset();
		Patch.OutlineMode = Value;
		Rejects(
		    [&]
		    {
			    ValidateViewportOptions(Patch, Supported);
		    });
	}
	bool bUnavailable{};
	try
	{
		FSceneViewportOptions Patch;
		Patch.OutlineMode = 256;
		ValidateViewportOptions(Patch, {});
	}
	catch (const FSceneEditError& Error)
	{
		bUnavailable = Error.Code == SceneEditErrors::Unavailable;
	}
	HYP_CHECK(bUnavailable);
}
} // namespace

void RunViewportChoiceTests()
{
	FixedWireMeanings();
	ProfilingCategories();
	Presentation(SceneCullingOptions(), ParseSceneCullingMode, ToCullingWireValue);
	Presentation(OutlineOverlapOptions(), ParseOutlineOverlapMode, ToOutlineWireValue);
	InvalidChoices();
	ViewportPreflight();
}
