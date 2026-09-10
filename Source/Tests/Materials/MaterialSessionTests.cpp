#include "Hyperion/Renderer/RenderSession.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <source_location>
#include <thread>

using namespace Hyperion;

void RunSceneRetentionTests(FTaskSystem& InTasks, const std::shared_ptr<const FRenderResource>& InResource);

namespace
{
void WaitFor(const std::function<bool()>& InCondition,
             const std::source_location& InLocation = std::source_location::current())
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (!InCondition())
	{
		if (std::chrono::steady_clock::now() > Deadline)
		{
			throw std::runtime_error("Material session readiness timed out at line " +
			                         std::to_string(InLocation.line()));
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

std::shared_ptr<const FMaterialSnapshot> Surface(std::shared_ptr<const FMaterialSemanticRegistry> InRegistry,
                                                 bool bInExtra = false)
{
	FMaterialDescription Description;
	Description.Name = "View family integration";
	FMaterialPass Pass;
	Pass.Vertex = {"Session.hlsl", "VSMain"};
	Pass.Pixel = {"Session.hlsl", "PSMain", {{"NEED_EXTRA", bInExtra ? "1" : "0"}}};
	Pass.State.bDepthTest = true;
	Pass.State.bDepthWrite = true;
	Description.Passes.push_back(Pass);
	Description.Parameters = {DeclareMaterialSemantic("World", "Engine.Object.World", *InRegistry),
	                          DeclareMaterialSemantic("ViewProjection", "Engine.View.ViewProjection", *InRegistry),
	                          DeclareMaterialSemantic("Camera", "Engine.View.CameraPosition", *InRegistry)};
	if (bInExtra)
	{
		Description.Parameters.push_back(DeclareMaterialSemantic("Extra", "Test.Scene.Extra", *InRegistry));
	}
	FMaterialParameterDeclaration Tint;
	Tint.Name = "Tint";
	Tint.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float, 4);
	Tint.Default = FMaterialValue::Float(FVec4{.2f, .1f, .1f, 1});
	Description.Parameters.push_back(Tint);
	return FMaterialInstance(std::make_shared<const FMaterialDefinition>(Description, InRegistry)).Freeze();
}

FRenderResourceDesc Geometry(std::shared_ptr<const FMaterialSnapshot> InSurface)
{
	FRenderResourceDesc Result;
	FRenderGeometryDesc Geometry;
	const std::array<FVec3, 3> Vertices{{{-1, -1, .5f}, {3, -1, .5f}, {-1, 3, .5f}}};
	const auto Bytes = std::as_bytes(std::span(Vertices));
	Geometry.Vertices.assign(Bytes.begin(), Bytes.end());
	Geometry.Indices = {0, 1, 2};
	Geometry.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0}};
	Geometry.VertexStride = sizeof(FVec3);
	Geometry.Bounds = {{-1, -1, .5f}, {3, 3, .5f}, true};
	Result.Geometries.push_back(std::move(Geometry));
	FRenderMaterialDesc Material;
	Material.Surface = std::move(InSurface);
	Result.Materials.push_back(std::move(Material));
	Result.Sections.push_back({0, 0, 0, 3});
	return Result;
}

std::vector<FPassCommands> Build(FTaskSystem& InTasks, FRenderSession& InSession, std::span<const FRenderView> InViews)
{
	std::vector<FPassCommands> Result;
	const auto Frame = InSession.FreezeFrame(1);
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              FRenderGraph Graph;
		                              FColorPass Clear;
		                              Clear.Commands.Name = "Clear session";
		                              Clear.Load = EColorLoad::Clear;
		                              Graph.Add(std::move(Clear));
		                              InSession.BuildViews(Graph, InViews, Frame);
		                              Result = Graph.Compile();
	                              }));
	return Result;
}

struct FEmission
{
	bool bStable = true;
	bool bReverse{};
	bool bChanged{};
	bool bDynamic{};
	bool bDrawParameters = true;
	float Offset{};
	bool bNegativeZero{};
	std::uint64_t Count = 2;
	std::uint64_t FirstId{};
};

class FMultipleItems final : public IRenderPrimitive
{
public:
	FMultipleItems(FTaskSystem& InTasks, std::shared_ptr<FEmission> InEmission)
	    : IRenderPrimitive(InTasks), Emission(std::move(InEmission))
	{
	}

	void Collect(const FRenderView&, std::vector<FRenderItem>& OutItems) const override
	{
		for (std::uint64_t Ordinal = 0; Ordinal < Emission->Count; ++Ordinal)
		{
			const auto Index = Emission->FirstId + (Emission->bReverse ? Emission->Count - 1 - Ordinal : Ordinal);
			FRenderItem Item;
			Item.State = GetState();
			Item.State.World = Translation({float(Index) * (Emission->bChanged ? .2f : .1f) + Emission->Offset, 0, 0});
			Item.State.World.Values[1] = Emission->bNegativeZero ? -0.f : 0.f;
			if (Emission->bDrawParameters)
			{
				Item.DrawParameters = {
				    {"Tint", FMaterialValue::Float(FVec4{float(Index) * .2f, Emission->bChanged ? .5f : 0, 0, 1})}};
			}
			if (Emission->bStable)
			{
				Item.LocalItemId = Index;
			}
			if (Emission->bDynamic)
			{
				Item.DynamicState = FMaterialDynamicState{17, {.2f, .3f, .4f, .5f}};
			}
			OutItems.push_back(std::move(Item));
		}
	}

private:
	std::shared_ptr<FEmission> Emission;
};

void CheckMultipleItems(FTaskSystem& InTasks, FRenderSession& InSession, FRenderPrimitiveState InState,
                        FRenderView InView)
{
	auto Emission = std::make_shared<FEmission>();
	auto Binding = InSession.GetScene().Create(InState,
	                                           [Emission](FTaskSystem& InTasks)
	                                           {
		                                           return std::make_unique<FMultipleItems>(InTasks, Emission);
	                                           });
	InTasks.Wait(InSession.GetScene().Flush());
	auto First = Build(InTasks, InSession, std::span(&InView, 1));
	HYP_CHECK(First[1].Draws.size() == 2);
	HYP_CHECK(First[1].Draws[0].ConstantBindings[1].Slice != First[1].Draws[1].ConstantBindings[1].Slice);
	HYP_CHECK(First[1].Draws[0].ConstantBindings[2].Slice != First[1].Draws[1].ConstantBindings[2].Slice);
	Emission->bReverse = true;
	auto Reversed = Build(InTasks, InSession, std::span(&InView, 1));
	for (std::size_t Index = 0; Index < 2; ++Index)
	{
		for (std::size_t Slot = 0; Slot < 2; ++Slot)
		{
			HYP_CHECK(First[1].Draws[Index].ConstantBindings[Slot].Slice ==
			          Reversed[1].Draws[1 - Index].ConstantBindings[Slot].Slice);
		}
	}
	Emission->bChanged = true;
	auto Changed = Build(InTasks, InSession, std::span(&InView, 1));
	HYP_CHECK(Changed[1].Draws[0].ConstantBindings[1].Slice != Reversed[1].Draws[0].ConstantBindings[1].Slice);
	HYP_CHECK(Changed[1].Draws[0].ConstantBindings[2].Slice != Reversed[1].Draws[0].ConstantBindings[2].Slice);
	Emission->bStable = false;
	auto Anonymous = Build(InTasks, InSession, std::span(&InView, 1));
	auto Again = Build(InTasks, InSession, std::span(&InView, 1));
	HYP_CHECK(Anonymous[1].Draws[0].ConstantBindings[1].Slice != Again[1].Draws[0].ConstantBindings[1].Slice);
	Emission->bDynamic = true;
	const auto Denied = Build(InTasks, InSession, std::span(&InView, 1));
	HYP_CHECK(Denied.size() == 2 && !Binding.GetLastDrawResult().Error.empty());
	auto Description = InState.Resource->GetMaterial(0)->GetSnapshot()->Definition->GetDescription();
	Description.Passes[0].bAllowDynamicOverrides = true;
	InState.Surface = InSession.GetResources().RequestMaterial(
	    FMaterialInstance(std::make_shared<const FMaterialDefinition>(Description)).Freeze());
	WaitFor(
	    [&]
	    {
		    return InState.Surface->GetStatus() == ERenderMaterialStatus::Ready;
	    });
	++InState.Revision;
	InTasks.Wait(InSession.GetScene().Update({{Binding.GetHandle(), InState}}));
	const auto Before = InSession.GetResources().Statistics().Materials.PipelinesCreated;
	const auto Allowed = Build(InTasks, InSession, std::span(&InView, 1));
	HYP_CHECK(Allowed[1].Draws[0].DynamicState.StencilReference == 17);
	HYP_CHECK(Allowed[1].Draws[0].DynamicState.BlendConstants[0] == .2f);
	HYP_CHECK(InSession.GetResources().Statistics().Materials.PipelinesCreated == Before);
	InTasks.Wait(Binding.Remove());
}

void CheckEvaluationCapacity(FTaskSystem& InTasks, FRenderSession& InSession, const FRenderPrimitiveState& InState,
                             FRenderView InView)
{
	InView.CullingMode = ESceneCullingMode::None;
	for (const std::uint64_t Count : {64, 65, 128})
	{
		auto Emission = std::make_shared<FEmission>();
		Emission->Count = Count;
		Emission->bDrawParameters = false;
		auto Binding = InSession.GetScene().Create(InState,
		                                           [Emission](FTaskSystem& InTasks)
		                                           {
			                                           return std::make_unique<FMultipleItems>(InTasks, Emission);
		                                           });
		InTasks.Wait(InSession.GetScene().Flush());
		for (unsigned WorkingSet = 0; WorkingSet < 2; ++WorkingSet)
		{
			// Replacing every local item must admit the new working set and retire the old one.
			Emission->FirstId = WorkingSet * Count;
			Build(InTasks, InSession, std::span(&InView, 1));
			for (unsigned Frame = 0; Frame < 3; ++Frame)
			{
				const auto Before = InSession.GetResources().Statistics().Materials;
				// The final frame also checks that incremental refresh keeps hot Object owners admitted.
				InView.Eye.X += Frame == 2 ? .001f : 0.f;
				const auto Passes = Build(InTasks, InSession, std::span(&InView, 1));
				HYP_CHECK(Passes[1].Draws.size() == Count);
				const auto After = InSession.GetResources().Statistics().Materials;
				HYP_CHECK(After.SetReuses - Before.SetReuses == Count - 64);
				HYP_CHECK(After.PipelineReuses - Before.PipelineReuses == Count - 64);
			}
		}
		InTasks.Wait(Binding.Remove());
	}
}

void CheckChangingObjectRetirement(FTaskSystem& InTasks, FRenderSession& InSession,
                                   const FRenderPrimitiveState& InState, FRenderView InView)
{
	auto Emission = std::make_shared<FEmission>();
	Emission->bDrawParameters = false;
	auto Binding = InSession.GetScene().Create(InState,
	                                           [Emission](FTaskSystem& InTasks)
	                                           {
		                                           return std::make_unique<FMultipleItems>(InTasks, Emission);
	                                           });
	InTasks.Wait(InSession.GetScene().Flush());
	InView.CullingMode = ESceneCullingMode::None;
	const auto Before = InSession.GetResources().Statistics().Constants.Packs;
	for (unsigned Index = 0; Index < 64; ++Index)
	{
		Emission->Offset = float(Index) * .01f;
		const auto Passes = Build(InTasks, InSession, std::span(&InView, 1));
		HYP_CHECK(Passes[1].Draws.size() == 2);
	}
	HYP_CHECK(InSession.GetResources().Statistics().Constants.Packs >= Before + 128);
	// Float sign bits remain observable to shader asuint even when C++ numeric equality says equal.
	const auto PositiveZero = InSession.GetResources().Statistics().Constants.Packs;
	Emission->bNegativeZero = true;
	Build(InTasks, InSession, std::span(&InView, 1));
	HYP_CHECK(InSession.GetResources().Statistics().Constants.Packs == PositiveZero + 2);
	// No primitive Update/Remove and no further frame: only the current Object contents may remain cached.
	WaitFor(
	    [&]
	    {
		    // The independent second view retains one additional View block.
		    return InSession.GetResources().Statistics().Constants.CachedBlocks <= 7;
	    });
	HYP_CHECK(Binding.GetStatus().Revision == InState.Revision);
	InTasks.Wait(Binding.Remove());
}

void CheckBoundsContract(FTaskSystem& InTasks, FRenderSession& InSession,
                         std::shared_ptr<const FRenderResource> InResource, FRenderView InView)
{
	FRenderPrimitiveState State;
	State.Resource = InResource;
	State.World = Translation({100, 0, 0});
	auto Binding = InSession.GetScene().Create(State);
	InTasks.Wait(InSession.GetScene().Flush());
	HYP_CHECK(Build(InTasks, InSession, std::span(&InView, 1)).size() == 2);
	auto Description = InResource->GetMaterial(0)->GetSnapshot()->Definition->GetDescription();
	Description.Passes[0].bRequiresConservativeBounds = true;
	auto Displaced = FMaterialInstance(std::make_shared<const FMaterialDefinition>(Description)).Freeze();
	State.Surface = InSession.GetResources().RequestMaterial(Displaced);
	WaitFor(
	    [&]
	    {
		    return State.Surface->GetStatus() == ERenderMaterialStatus::Ready;
	    });
	State.Revision++;
	InTasks.Wait(InSession.GetScene().Update({{Binding.GetHandle(), State}}));
	HYP_CHECK(Build(InTasks, InSession, std::span(&InView, 1))[1].Draws.size() == 1);
	State.bConservativeBounds = true;
	State.Revision++;
	InTasks.Wait(InSession.GetScene().Update({{Binding.GetHandle(), State}}));
	HYP_CHECK(Build(InTasks, InSession, std::span(&InView, 1)).size() == 2);
	State.Surface.reset();
	State.World = Identity();
	State.bClipSpace = true;
	State.Revision++;
	InView.ViewProjection = Translation({100, 0, 0});
	InTasks.Wait(InSession.GetScene().Update({{Binding.GetHandle(), State}}));
	HYP_CHECK(Build(InTasks, InSession, std::span(&InView, 1))[1].Draws.size() == 1);
	InTasks.Wait(Binding.Remove());
}

void CheckFamilyValidation(FTaskSystem& InTasks, FRenderSession& InSession, FRenderView InView)
{
	const auto Frame = InSession.FreezeFrame();
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              FRenderGraph Graph;
		                              const std::array Duplicate{InView, InView};
		                              bool bRejected{};
		                              try
		                              {
			                              InSession.BuildViews(Graph, Duplicate, Frame);
		                              }
		                              catch (const std::invalid_argument&)
		                              {
			                              bRejected = true;
		                              }
		                              HYP_CHECK(bRejected);
		                              InSession.BuildViews(Graph, std::span(&InView, 1), Frame);
		                              bRejected = false;
		                              try
		                              {
			                              InSession.BuildViews(Graph, std::span(&InView, 1), Frame);
		                              }
		                              catch (const std::invalid_argument&)
		                              {
			                              bRejected = true;
		                              }
		                              HYP_CHECK(bRejected);
	                              }));
}

void CheckIndependentFamilies(FTaskSystem& InTasks, FRenderSession& InSession, std::array<FRenderView, 2> InViews)
{
	for (unsigned Iteration = 0; Iteration < 3; ++Iteration)
	{
		const auto Frame = InSession.FreezeFrame();
		InTasks.Wait(InTasks.Dispatch({EDomain::Render},
		                              [&]
		                              {
			                              for (std::size_t Index = 0; Index < InViews.size(); ++Index)
			                              {
				                              FRenderGraph Graph;
				                              InSession.BuildViews(Graph, std::span(&InViews[Index], 1), Frame,
				                                                   100 + Index);
				                              if (Iteration)
				                              {
					                              HYP_CHECK(InSession.Statistics().PreparationReuses == 1);
					                              HYP_CHECK(InSession.Statistics().PacketReuses == 1);
				                              }
			                              }
		                              }));
	}
	for (auto& View : InViews)
	{
		View.ViewProjection = Scale({.98f, .98f, 1});
		View.Eye.X += .02f;
	}
	Build(InTasks, InSession, InViews);
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              for (const auto& View : InSession.ViewStatistics())
		                              {
			                              HYP_CHECK(View.Visibility.ItemPreparationReuses == 2);
			                              HYP_CHECK(View.Visibility.ItemStorageReuses == 2);
			                              HYP_CHECK(View.Visibility.SharedMaterialUpdates == 2);
		                              }
	                              }));
}

void Pixel(const FImage& InImage, std::size_t InX, float InRed)
{
	HYP_CHECK(std::abs(InImage.Rgba[(32 * 64 + InX) * 4] - InRed) < .01f);
}

std::filesystem::path SessionShader()
{
	const auto Root = std::filesystem::absolute("material-session-test/source");
	std::filesystem::create_directories(Root);
	std::ofstream(Root / "Session.hlsl") << R"(
cbuffer ViewData : register(b0) { float4x4 ViewProjection; float3 Camera; };
cbuffer ObjectData : register(b1) { float4x4 World; };
cbuffer SurfaceData : register(b2) { float4 Tint; };
#if NEED_EXTRA
cbuffer SceneData : register(b3) { float Extra; };
#endif
#if NEED_TEXTURE
Texture2D<float4> ViewTexture : register(t0);
#endif
float4 VSMain(float3 InPosition : POSITION) : SV_Position { return mul(ViewProjection, mul(World, float4(InPosition, 1))) + float4(Camera.yz, 0, 0); }
float4 PSMain() : SV_Target0
{
    float4 Color = Tint + float4(Camera.x + ViewProjection[0][0] - 1, 0, 0, 0);
#if NEED_EXTRA
    Color.b += Extra;
#endif
#if NEED_TEXTURE
    Color *= ViewTexture.Load(int3(0, 0, 0));
#endif
    return Color;
}
)";
	return Root;
}

FImage RenderViews(FTaskSystem& InTasks, IRHISwapchain& InSwapchain, const std::vector<FPassCommands>& InPasses)
{
	FImage Image;
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              InSwapchain.BeginFrame({64, 64});
		                              std::vector<FRecordedList> Lists;
		                              std::uint32_t Context{};
		                              for (const auto& Pass : InPasses)
		                              {
			                              Lists.push_back(InSwapchain.Record(Context++, Pass));
		                              }
		                              Image = InSwapchain.EndFrame(Lists, false, true);
	                              }));
	return Image;
}

void CheckProviderFallback(FTaskSystem& InTasks, FRenderSession& InSession, IRHISwapchain& InSwapchain,
                           std::shared_ptr<const FMaterialSemanticRegistry> InSemantics,
                           const std::shared_ptr<const FRenderResource>& InResource, const FRenderView& InView)
{
	auto Description = Surface(InSemantics, true)->Definition->GetDescription();
	for (auto& Parameter : Description.Parameters)
	{
		if (Parameter.Name == "Extra")
		{
			Parameter.Default = FMaterialValue::Float(.05f);
		}
	}
	const auto Definition = std::make_shared<const FMaterialDefinition>(Description, InSemantics);
	FRenderPrimitiveState State;
	State.Resource = InResource;
	State.Surface = InSession.GetResources().RequestMaterial(FMaterialInstance(Definition).Freeze());
	auto Binding = InSession.GetScene().Create(State);
	WaitFor(
	    [&]
	    {
		    return Binding.GetStatus().State == ERenderPrimitiveStatus::Ready;
	    });
	InSession.SetSceneParameters({});
	auto Passes = Build(InTasks, InSession, std::span(&InView, 1));
	const auto First = RenderViews(InTasks, InSwapchain, Passes);
	HYP_CHECK(std::abs(First.Rgba[(32 * 64 + 16) * 4 + 2] - .15f) < .01f);
	InSession.SetSceneParameters({{"Test.Scene.Extra", FMaterialValue::Float(.25f)}});
	Passes = Build(InTasks, InSession, std::span(&InView, 1));
	const auto Changed = RenderViews(InTasks, InSwapchain, Passes);
	HYP_CHECK(std::abs(Changed.Rgba[(32 * 64 + 16) * 4 + 2] - .35f) < .01f);
	InSession.SetSceneParameters({});
	Passes = Build(InTasks, InSession, std::span(&InView, 1));
	const auto Restored = RenderViews(InTasks, InSwapchain, Passes);
	HYP_CHECK(std::abs(Restored.Rgba[(32 * 64 + 16) * 4 + 2] - .15f) < .01f);
	InTasks.Wait(Binding.Remove());
}

void CheckMovingViews(FTaskSystem& InTasks, FRenderSession& InSession, IRHISwapchain& InSwapchain,
                      std::array<FRenderView, 2> InViews)
{
	const auto Original = Build(InTasks, InSession, InViews);
	const auto OriginalEye = InViews[0].Eye.X;
	const auto Warm = InSession.GetResources().Statistics();
	std::vector<FPassCommands> Current;
	for (std::uint32_t Index = 0; Index < 128; ++Index)
	{
		InViews[0].Eye.X = .2f + float(Index % 20) * .01f;
		Current = Build(InTasks, InSession, InViews);
		InTasks.Wait(InTasks.Dispatch({EDomain::Render},
		                              [&]
		                              {
			                              const auto& View = InSession.ViewStatistics().front().Visibility;
			                              HYP_CHECK(View.RetainedMaterialItems == 2 && View.SharedMaterialGroups == 1);
		                              }));
		HYP_CHECK(Current[1].Draws.size() == 2 && Current[2].Draws.size() == 2);
		for (std::size_t Draw = 0; Draw < 2; ++Draw)
		{
			HYP_CHECK(Current[1].Draws[Draw].Pipeline == Original[1].Draws[Draw].Pipeline);
			HYP_CHECK(Current[1].Draws[Draw].ConstantBindings[1].Slice ==
			          Original[1].Draws[Draw].ConstantBindings[1].Slice);
			HYP_CHECK(Current[1].Draws[Draw].ConstantBindings[2].Slice ==
			          Original[1].Draws[Draw].ConstantBindings[2].Slice);
		}
	}
	const auto Final = InSession.GetResources().Statistics();
	HYP_CHECK(Final.Constants.Packs == Warm.Constants.Packs + 128);
	HYP_CHECK(Final.Materials.PipelineReuses == Warm.Materials.PipelineReuses);
	HYP_CHECK(Final.Materials.SetReuses == Warm.Materials.SetReuses);
	Pixel(RenderViews(InTasks, InSwapchain, Current), 16, .2f + InViews[0].Eye.X);
	// A packet retained across many view revisions must still see the original immutable upload bytes.
	Pixel(RenderViews(InTasks, InSwapchain, Original), 16, .2f + OriginalEye);
	WaitFor(
	    [&]
	    {
		    return InSession.GetResources().Statistics().Constants.CachedBlocks < 16;
	    });
	HYP_CHECK(InSession.GetResources().Statistics().Constants.LivePages <= 3);
}

void CheckViewResourceRefresh(FTaskSystem& InTasks, FRenderSession& InSession, IRHISwapchain& InSwapchain,
                              std::shared_ptr<const FMaterialSemanticRegistry> InSemantics,
                              const std::shared_ptr<const FRenderResource>& InResource, FRenderView InView)
{
	auto Description = Surface(InSemantics)->Definition->GetDescription();
	Description.Passes[0].Pixel.Defines.push_back({"NEED_TEXTURE", "1"});
	Description.Parameters.push_back(DeclareMaterialSemantic("ViewTexture", "Test.View.Texture", *InSemantics));
	FRenderPrimitiveState State;
	State.Resource = InResource;
	State.Surface = InSession.GetResources().RequestMaterial(
	    FMaterialInstance(std::make_shared<const FMaterialDefinition>(Description, InSemantics)).Freeze());
	auto Binding = InSession.GetScene().Create(State);
	WaitFor(
	    [&]
	    {
		    return Binding.GetStatus().State == ERenderPrimitiveStatus::Ready;
	    });
	const auto Texture = [](std::array<std::uint8_t, 4> InColor)
	{
		return FMaterialValue::FromTexture(std::make_shared<const FMaterialTextureSource>(
		    EMaterialTextureEncoding::Linear,
		    std::vector<FMaterialTextureMip>{{1, 1, {InColor.begin(), InColor.end()}}}));
	};
	const auto Red = Texture({255, 0, 0, 255});
	const auto Green = Texture({0, 255, 0, 255});
	const auto Ready = [&]
	{
		std::vector<FPassCommands> Passes;
		WaitFor(
		    [&]
		    {
			    Passes = Build(InTasks, InSession, std::span(&InView, 1));
			    return Passes.size() > 2 && Passes[1].Draws.size() == 1;
		    });
		return Passes;
	};
	InView.Parameters = {{"Test.View.Texture", Red}};
	const auto Original = Ready();
	Pixel(RenderViews(InTasks, InSwapchain, Original), 16, .2f + InView.Eye.X);
	InView.Parameters = {{"Test.View.Texture", Green}};
	auto Changed = Ready();
	HYP_CHECK(Changed[1].Draws[0].Bindings != Original[1].Draws[0].Bindings);
	const auto Image = RenderViews(InTasks, InSwapchain, Changed);
	Pixel(Image, 16, 0);
	HYP_CHECK(std::abs(Image.Rgba[(32 * 64 + 16) * 4 + 1] - .1f) < .01f);
	InView.Parameters.clear();
	HYP_CHECK(Build(InTasks, InSession, std::span(&InView, 1)).size() == 2);
	HYP_CHECK(!Binding.GetLastDrawResult().bReady);
	InView.Parameters = {{"Test.View.Texture", Red}};
	Changed = Ready();
	Pixel(RenderViews(InTasks, InSwapchain, Changed), 16, .2f + InView.Eye.X);
	Pixel(RenderViews(InTasks, InSwapchain, Original), 16, .2f + InView.Eye.X);
	InTasks.Wait(Binding.Remove());
}
} // namespace

void RunMaterialSessionTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	const auto Root = SessionShader();
	auto Semantics = std::make_shared<FMaterialSemanticRegistry>();
	Semantics->Register({"Test.Scene.Extra", FMaterialParameterType::Numeric(EMaterialScalar::Float),
	                     EMaterialScope::Scene, "Linear blue contribution"});
	Semantics->Register({"Test.View.Texture", FMaterialParameterType::Resource(EMaterialValueKind::Texture2D),
	                     EMaterialScope::View, "View resource refresh"});
	Semantics->Freeze();
	FTaskSystem Tasks(1, 1);
	FShaderCompiler Compiler(Root, "material-session-test/cache");
	FRenderSession Session(Tasks, InDevice, Compiler, ERHIDepthFormat::D32S8, Semantics);
	const auto Snapshot = Surface(Semantics);
	auto Resource = Session.GetResources().Request(Snapshot, 1, "session-test",
	                                               [Snapshot]
	                                               {
		                                               return Geometry(Snapshot);
	                                               });
	FRenderPrimitiveState State;
	State.Resource = Resource;
	auto Bindings = Session.GetScene().CreateBatch({State, State});
	WaitFor(
	    [&]
	    {
		    return Bindings[0].GetStatus().State == ERenderPrimitiveStatus::Ready;
	    });
	HYP_CHECK(Session.GetResources().Statistics().Constants.PagesCreated == 0);
	std::array<FRenderView, 2> Views;
	for (std::size_t Index = 0; Index < Views.size(); ++Index)
	{
		Views[Index].Identity = Index + 1;
		Views[Index].Eye.X = Index == 0 ? .1f : .6f;
		Views[Index].Width = Views[Index].Height = 64;
		Views[Index].Viewport = FViewport{float(Index * 32), 0, 32, 64};
	}
	auto Passes = Build(Tasks, Session, Views);
	HYP_CHECK(Passes.size() == 4 && Passes[1].Name != Passes[2].Name);
	HYP_CHECK(Passes[1].Draws.size() == 2 && Passes[2].Draws.size() == 2);
	const auto& First = Passes[1].Draws;
	const auto& Second = Passes[2].Draws;
	HYP_CHECK(First[0].ConstantBindings[0].Slice == First[1].ConstantBindings[0].Slice);
	HYP_CHECK(First[0].ConstantBindings[0].Slice != Second[0].ConstantBindings[0].Slice);
	HYP_CHECK(First[0].ConstantBindings[1].Slice == Second[0].ConstantBindings[1].Slice);
	HYP_CHECK(First[0].ConstantBindings[2].Slice == Second[1].ConstantBindings[2].Slice);
	const auto Image = RenderViews(Tasks, InSwapchain, Passes);
	Pixel(Image, 16, .3f);
	Pixel(Image, 48, .8f);
	const auto Warm = Session.GetResources().Statistics();
	HYP_CHECK(Warm.Constants.Packs == 5 && Warm.Materials.PipelinesCreated == 1);
	Passes = Build(Tasks, Session, Views);
	HYP_CHECK(Session.GetResources().Statistics().Constants.Packs == Warm.Constants.Packs);
	Views[0].Eye.X = .15f;
	Passes = Build(Tasks, Session, Views);
	HYP_CHECK(Session.GetResources().Statistics().Constants.Packs == Warm.Constants.Packs + 1);
	HYP_CHECK(Session.GetResources().Statistics().Materials.SetsCreated == Warm.Materials.SetsCreated);
	CheckMovingViews(Tasks, Session, InSwapchain, Views);
	CheckIndependentFamilies(Tasks, Session, Views);
	auto Required = Session.GetResources().RequestMaterial(Surface(Semantics, true));
	WaitFor(
	    [&]
	    {
		    return Required->GetStatus() == ERenderMaterialStatus::Ready;
	    });
	State.Surface = Required;
	State.Revision = 2;
	Tasks.Wait(Session.GetScene().Update({{Bindings[0].GetHandle(), State}}));
	FRenderPrimitiveState Independent;
	Independent.Resource = Resource;
	auto Other = Session.GetScene().Create(Independent);
	Tasks.Wait(Session.GetScene().Flush());
	Passes = Build(Tasks, Session, std::span(Views).first(1));
	HYP_CHECK(Passes[1].Draws.size() == 1);
	HYP_CHECK(!Bindings[0].GetLastDrawResult().bReady && !Bindings[1].GetLastDrawResult().bReady);
	HYP_CHECK(Other.GetLastDrawResult().bReady && Bindings[0].GetStatus().State == ERenderPrimitiveStatus::Ready);
	Session.SetSceneParameters({{"Test.Scene.Extra", FMaterialValue::Float(.25f)}});
	Passes = Build(Tasks, Session, std::span(Views).first(1));
	HYP_CHECK(Passes[1].Draws.size() == 3 && Bindings[0].GetLastDrawResult().bReady);
	Passes.clear();
	Bindings.clear();
	Other.Remove();
	Tasks.Wait(Session.GetScene().Flush());
	State = {};
	State.Resource = Resource;
	CheckMultipleItems(Tasks, Session, State, Views[0]);
	CheckEvaluationCapacity(Tasks, Session, State, Views[0]);
	CheckChangingObjectRetirement(Tasks, Session, State, Views[0]);
	CheckFamilyValidation(Tasks, Session, Views[0]);
	CheckBoundsContract(Tasks, Session, Resource, Views[0]);
	CheckProviderFallback(Tasks, Session, InSwapchain, Semantics, Resource, Views[0]);
	CheckViewResourceRefresh(Tasks, Session, InSwapchain, Semantics, Resource, Views[0]);
	RunSceneRetentionTests(Tasks, Resource);
	State = {};
	Independent = {};
	Required.reset();
	Resource.reset();
	Session.Close();
	const auto Closed = Session.GetResources().Statistics().Constants;
	HYP_CHECK(Closed.LivePages == 0 && Closed.PageBytes == 0);
}
