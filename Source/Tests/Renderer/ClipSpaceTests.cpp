#include "Hyperion/Renderer/RenderSession.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <cmath>
#include <thread>

using namespace Hyperion;

namespace
{
FRenderResourceDesc ClipGeometry()
{
	FMaterialDescription Description;
	Description.Name = "Clip-space depth and facing";
	FMaterialPass Pass;
	Pass.Vertex = {"ClipSpace.hlsl", "VSMain"};
	Pass.Pixel = {"ClipSpace.hlsl", "PSMain"};
	Pass.InstanceArrays = {{"ClipData", "Instances"}};
	Pass.bAllowBatchReordering = true;
	Pass.State.Cull = EMaterialCull::Back;
	Pass.State.bDepthTest = true;
	Pass.State.bDepthWrite = true;
	Pass.State.bViewRelativeDepth = true;
	Description.Passes.push_back(Pass);
	Pass.Usage = "FrontCull";
	Pass.State.Cull = EMaterialCull::Front;
	Description.Passes.push_back(Pass);
	Pass.Usage = "TwoSided";
	Pass.State.Cull = EMaterialCull::None;
	Description.Passes.push_back(Pass);
	Pass.Usage = "Transparent";
	Pass.State.Cull = EMaterialCull::Back;
	Pass.State.bDepthWrite = false;
	Pass.State.bBlend = true;
	Pass.State.SourceRgb = EMaterialBlendFactor::SourceAlpha;
	Pass.State.DestinationRgb = EMaterialBlendFactor::InverseSourceAlpha;
	Pass.Queue = EMaterialQueue::Transparent;
	Description.Passes.push_back(Pass);
	for (const auto& [Name, Semantic] : {std::pair{"Transform", "Engine.Object.WorldViewProjection"},
	                                     std::pair{"Orientation", "Engine.Object.OrientationSign"}})
	{
		auto Parameter = DeclareMaterialSemantic(Name, Semantic, *GetStandardMaterialSemantics());
		Parameter.Targets = {std::string("ClipData.") + Name};
		Description.Parameters.push_back(std::move(Parameter));
	}
	FMaterialParameterDeclaration Tint;
	Tint.Name = "Tint";
	Tint.Default = FMaterialValue::Float(FVec4{.2f, .8f, .1f, 1});
	Tint.Type = Tint.Default->Type;
	Tint.Targets = {"ClipData.Tint"};
	Description.Parameters.push_back(Tint);
	FRenderResourceDesc Result;
	Result.Materials.push_back(
	    {FMaterialInstance(std::make_shared<const FMaterialDefinition>(std::move(Description))).Freeze()});
	FRenderGeometryDesc Geometry;
	const std::array<FVec3, 3> Vertices{{{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}};
	const auto Bytes = std::as_bytes(std::span(Vertices));
	Geometry.Vertices.assign(Bytes.begin(), Bytes.end());
	Geometry.Indices = {0, 1, 2};
	Geometry.VertexStride = sizeof(FVec3);
	Geometry.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0}};
	Geometry.Bounds = {{-1, -1, 0}, {1, 1, 0}, true};
	Result.Geometries.push_back(std::move(Geometry));
	Result.Sections.push_back({0, 0, 0, 3});
	return Result;
}

struct FClipFixture
{
	FTaskSystem& Tasks;
	IRHISwapchain& Swapchain;
	FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "Source/Tests/Renderer", "clip-test/cache"};
	FRenderSession Session;
	std::shared_ptr<const FRenderResource> Resource;
	std::vector<FRenderBinding> Bindings;
	std::shared_ptr<const void> Lifetime;
	std::array<std::shared_ptr<const FMaterialTextureSource>, 2> Depth;
	FSceneVisibilityStats Statistics;

	FClipFixture(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
	    : Tasks(InTasks), Swapchain(InSwapchain), Session(InTasks, InDevice, Compiler)
	{
		Resource = Session.GetResources().Request(std::make_shared<const int>(0), 1, "clip-space", ClipGeometry);
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
		while (Resource->GetStatus() != ERenderResourceStatus::Ready &&
		       Resource->GetStatus() != ERenderResourceStatus::Failed && std::chrono::steady_clock::now() < Deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		HYP_CHECK(Resource->GetStatus() == ERenderResourceStatus::Ready);
		const auto Material = Resource->GetMaterial(0);
		while (Material->GetStatus() != ERenderMaterialStatus::Ready &&
		       Material->GetStatus() != ERenderMaterialStatus::Failed && std::chrono::steady_clock::now() < Deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (Material->GetStatus() != ERenderMaterialStatus::Ready)
		{
			throw std::runtime_error(Material->GetError());
		}
		HYP_CHECK(Material->GetCompiled()->FindInstancePass());
		Lifetime = Session.GetResources().CreateScopeLifetime();
		Depth = {std::make_shared<const FMaterialTextureSource>(FMaterialDepthTexture{384, 288, 1}),
		         std::make_shared<const FMaterialTextureSource>(FMaterialDepthTexture{384, 288, 0})};
	}

	~FClipFixture()
	{
		Bindings.clear();
		Resource.reset();
		Session.Close();
	}

	void Add(FVec3 InPosition, float InScaleX, FVec4 InTint = {.2f, .8f, .1f, 1})
	{
		FRenderPrimitiveState State;
		State.Resource = Resource;
		State.bClipSpace = true;
		State.World = Multiply(Translation(InPosition), Scale({InScaleX, .28f, 1}));
		State.ObjectParameters = {{"Tint", FMaterialValue::Float(InTint)}};
		Bindings.push_back(Session.GetScene().Create(State));
	}

	FImage Frame(EDepthConvention InConvention, bool bInBatch, std::string InUsage = "Forward")
	{
		Tasks.Wait(Session.GetScene().Flush());
		const auto Frame = Session.FreezeFrame();
		FRenderView View;
		View.Width = 384;
		View.Height = 288;
		View.DepthConvention = InConvention;
		View.bInstanceBatching = bInBatch;
		View.Usage = std::move(InUsage);
		FRenderPassTargets Targets = FRenderPassTargets::ColorOnly(FVec4{0, 0, 0, 1});
		Targets.DepthStencil = FRenderDepthTarget{
		    {ERenderTargetKind::Texture, Depth[static_cast<std::size_t>(InConvention)], Lifetime, false},
		    ERHIDepthFormat::D32,
		    FAttachmentActions{EAttachmentLoad::Clear},
		    {},
		    GetDepthClearValue(InConvention)};
		FImage Image;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          FRenderGraph Graph;
			                          Session.BuildViews(Graph, std::span(&View, 1), Targets, Frame, 1, false, true);
			                          const auto Preparation = Session.GetViewPreparation();
			                          Image = ExecuteGraph(std::move(Graph), Tasks, Swapchain, {384, 288}, false, true);
			                          Statistics = Preparation.Statistics().Views.at(0).Visibility;
			                          HYP_CHECK(Statistics.Batches.FailedItems == 0);
		                          }));
		return Image;
	}
};

void CheckColor(const FImage& InImage, unsigned InX, unsigned InY, FVec3 InExpected)
{
	const auto Index = (InY * InImage.Width + InX) * 4;
	HYP_CHECK(std::abs(InImage.Rgba.at(Index) - InExpected.X) < .008f);
	HYP_CHECK(std::abs(InImage.Rgba.at(Index + 1) - InExpected.Y) < .008f);
	HYP_CHECK(std::abs(InImage.Rgba.at(Index + 2) - InExpected.Z) < .008f);
}

void CheckFacing(FClipFixture& InFixture)
{
	InFixture.Add({-.5f, .45f, .25f}, .28f);
	InFixture.Add({.5f, .45f, .25f}, .28f);
	InFixture.Add({-.5f, -.45f, .25f}, -.28f);
	InFixture.Add({.5f, -.45f, .25f}, -.28f);
	for (const bool bBatch : {false, true})
	{
		for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
		{
			for (const auto* Usage : {"Forward", "TwoSided", "FrontCull"})
			{
				const auto Image = InFixture.Frame(Convention, bBatch, Usage);
				HYP_CHECK(InFixture.Statistics.Batches.InstancedItems == (bBatch ? 4 : 0));
				for (const unsigned X : {96u, 288u})
				{
					CheckColor(Image, X, 79, std::string_view(Usage) == "FrontCull" ? FVec3{} : FVec3{.2f, .8f, .1f});
					CheckColor(Image, X, 209, std::string_view(Usage) == "FrontCull" ? FVec3{} : FVec3{.2f, .4f, .1f});
				}
			}
		}
	}
	InFixture.Bindings.clear();
}

void CheckDepthAndSorting(FClipFixture& InFixture)
{
	InFixture.Add({0, 0, .25f}, .6f, {1, 0, 0, .5f});
	InFixture.Add({0, 0, .75f}, .6f, {0, 0, 1, .5f});
	// Matrices, primitive state and view revision stay fixed as only the depth convention changes.
	for (const bool bBatch : {false, true})
	{
		for (const auto* Usage : {"Forward", "Transparent"})
		{
			for (const auto Convention :
			     {EDepthConvention::Standard, EDepthConvention::Reversed, EDepthConvention::Standard})
			{
				const auto Image = InFixture.Frame(Convention, bBatch, Usage);
				CheckColor(Image, 192, 144,
				           std::string_view(Usage) == "Forward" ? FVec3{1, 0, 0} : FVec3{.5f, 0, .25f});
			}
		}
	}
	InFixture.Bindings.clear();
	InFixture.Add({0, 0, -.1f}, .6f);
	InFixture.Add({0, 0, 1.1f}, .6f);
	for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
	{
		CheckColor(InFixture.Frame(Convention, true), 192, 144, {});
		HYP_CHECK(InFixture.Statistics.VisibleItems == 0);
	}
}
} // namespace

void RunClipSpaceTests(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	FClipFixture Fixture(InTasks, InDevice, InSwapchain);
	CheckFacing(Fixture);
	CheckDepthAndSorting(Fixture);
}
