#include "Hyperion/Renderer/ClusteredLights.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

using namespace Hyperion;

namespace
{
const FMaterialValue& Parameter(const FClusterLightFrame& InFrame, const std::string& InName)
{
	const auto It = std::find_if(InFrame.Parameters.begin(), InFrame.Parameters.end(),
	                             [&](const auto& InEntry)
	                             {
		                             return InEntry.Name == "Engine.View." + InName;
	                             });
	HYP_CHECK(It != InFrame.Parameters.end());
	return It->Value;
}

template<typename T> std::vector<T> Records(const FClusterLightFrame& InFrame, const std::string& InName)
{
	const auto Bytes = Parameter(InFrame, InName).Buffer.Source->GetBytes();
	HYP_CHECK(Bytes.size() % sizeof(T) == 0);
	std::vector<T> Result(Bytes.size() / sizeof(T));
	std::memcpy(Result.data(), Bytes.data(), Bytes.size());
	return Result;
}

void CheckCoverage(EDepthConvention InConvention)
{
	FRenderView View;
	View.Eye = {2, 1, 5};
	View.Width = 513;
	View.Height = 377;
	View.Viewport = FViewport{17, 23, 479, 339, .2f, .85f};
	View.Camera = FRenderCamera{Normalize(Subtract({0, 0, 0}, View.Eye)), {0, 1, 0}, 1, .1f, 40};
	View.DepthConvention = InConvention;
	View.ViewProjection = Multiply(Perspective(1, 479.f / 339, .1f, 40, InConvention), LookAt(View.Eye, {}));
	FSceneMetadata Metadata;
	Metadata.Token = {1, 1, 1};
	Metadata.LocalLightRevision = 1;
	for (unsigned Index = 0; Index < 12; ++Index)
	{
		Metadata.PointLights.emplace(
		    FSceneHandle{1, Index, 1},
		    FPublishedPointLight{
		        {{1, 1, 1}, 5, 3}, {float(Index % 4) * 2 - 3, float(Index / 4) * 2 - 2, float(Index) - 5}, true});
	}
	FLocalLightIndex LightIndex;
	FLocalLightStatistics Stats;
	const auto Lights = LightIndex.Query(Metadata, nullptr, true, Stats);
	FClusteredLights Builder;
	const auto Frame = Builder.Build(View, Lights);
	const auto Headers = Records<FClusterHeader>(Frame, "ClusterHeaders");
	const auto Indices = Records<std::uint32_t>(Frame, "ClusterIndices");
	const auto InverseView = Inverse(View.ViewProjection);
	std::mt19937 Random(7123);
	std::uniform_real_distribution<float> Unit(0, 1);
	for (unsigned Sample = 0; Sample < 6000; ++Sample)
	{
		const float X = Unit(Random) * 479;
		const float Y = Unit(Random) * 339;
		const float Depth = .1f * std::pow(400.f, Unit(Random));
		const auto H = Transform(InverseView, {X / 479 * 2 - 1, 1 - Y / 339 * 2, .5f, 1});
		const auto Ray = Subtract({H.X / H.W, H.Y / H.W, H.Z / H.W}, View.Eye);
		const auto World = Add(View.Eye, ScaleVector(Ray, Depth / Dot(Ray, View.Camera->Forward)));
		const auto Z = std::min(23u, unsigned(std::floor(std::log2(Depth / .1f) * 24 / std::log2(400.f))));
		const auto& Header = Headers[unsigned(X / 64) + 8 * (unsigned(Y / 64) + 6 * Z)];
		HYP_CHECK(Header.Offset + Header.Count <= Indices.size());
		const auto Begin = Indices.begin() + Header.Offset;
		const auto End = Begin + Header.Count;
		HYP_CHECK(std::adjacent_find(Begin, End) == End);
		for (std::uint32_t Index = 0; Index < Lights.size(); ++Index)
		{
			if (Length(Subtract(World, Lights[Index].Position)) < Lights[Index].Range)
			{
				HYP_CHECK(std::find(Begin, End, Index) != End);
			}
		}
	}
	HYP_CHECK(!Builder.Build(View, Lights).Statistics.bRebuilt);
	auto Changed = Lights;
	Changed[0].Radiance.X += 1;
	const auto Edited = Builder.Build(View, Changed);
	HYP_CHECK(!Edited.Statistics.bRebuilt);
	HYP_CHECK(Parameter(Frame, "ClusterHeaders") == Parameter(Edited, "ClusterHeaders"));
	HYP_CHECK(Parameter(Frame, "ClusterLights") != Parameter(Edited, "ClusterLights"));
	Changed[0].Bounds.Maximum.X += 2;
	HYP_CHECK(Builder.Build(View, Changed).Statistics.bRebuilt);
	HYP_CHECK(Builder.Build(View, {}).Statistics.References == 0);
	HYP_CHECK(Records<FClusterHeader>(Frame, "ClusterHeaders").size() == Headers.size());
}

void CheckDense()
{
	FRenderView View;
	View.Width = 128;
	View.Height = 128;
	View.Camera = FRenderCamera{{0, 0, -1}, {0, 1, 0}, 1, .1f, 40};
	View.ViewProjection = Perspective(1, 1, .1f, 40);
	FLocalLight Light;
	Light.Range = 100;
	Light.Radiance = {1, 1, 1};
	Light.Bounds = {{-100, -100, -100}, {100, 100, 100}, true};
	std::vector<FLocalLight> Lights(128, Light);
	FClusteredLights Builder;
	const auto Frame = Builder.Build(View, Lights);
	HYP_CHECK(Frame.Statistics.MaximumLights == 128 && Frame.Statistics.References == 128 * 4 * 24);
	const auto Headers = Records<FClusterHeader>(Frame, "ClusterHeaders");
	const auto Indices = Records<std::uint32_t>(Frame, "ClusterIndices");
	for (const auto& Header : Headers)
	{
		HYP_CHECK(Header.Count == 128);
		for (unsigned Index = 0; Index < Header.Count; ++Index)
		{
			HYP_CHECK(Indices[Header.Offset + Index] == Index);
		}
	}
	View.Eye.X = .1f;
	View.ViewProjection = Multiply(View.ViewProjection, Translation({-.1f, 0, 0}));
	const auto Moved = Builder.Build(View, Lights);
	HYP_CHECK(Moved.Statistics.bRebuilt);
	HYP_CHECK(Parameter(Frame, "ClusterHeaders") == Parameter(Moved, "ClusterHeaders"));
	HYP_CHECK(Parameter(Frame, "ClusterIndices") == Parameter(Moved, "ClusterIndices"));
	View.Camera.reset();
	bool bRejected{};
	try
	{
		Builder.Build(View, Lights);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
} // namespace

void CheckClusteredLights()
{
	CheckCoverage(EDepthConvention::Standard);
	CheckCoverage(EDepthConvention::Reversed);
	CheckDense();
}
