#include "Hyperion/RasterOptions/RasterOptions.h"
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
		bUnavailable = Error.Code == "unavailable";
	}
	HYP_CHECK(bUnavailable);
}
} // namespace

void RunViewportChoiceTests()
{
	FixedWireMeanings();
	Presentation(SceneCullingOptions(), ParseSceneCullingMode, ToCullingWireValue);
	Presentation(OutlineOverlapOptions(), ParseOutlineOverlapMode, ToOutlineWireValue);
	InvalidChoices();
	ViewportPreflight();
}
