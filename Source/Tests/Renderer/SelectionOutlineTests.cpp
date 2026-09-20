#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/MaterialPreparation.h"
#include "Hyperion/Renderer/SceneBridge.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Renderer/SelectionOutline.h"
#include "Hyperion/Renderer/TransientGeometry.h"
#include "Support/ModelAssetSupport.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace Hyperion;

namespace
{
std::shared_ptr<const FSceneModelData> Quad(bool bInMasked = false, bool bInSplit = false)
{
	auto Source = std::make_shared<FModelSource>();
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
	Primitive.Normals = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
	Primitive.TexCoords0 = {0, 1, 1, 1, 1, 0, 0, 0};
	Primitive.Indices = {0, 1, 2, 0, 2, 3};
	Primitive.Material = 0;
	FModelMaterial Material;
	Material.bUnlit = true;
	Material.bDoubleSided = true;
	Material.BaseColor = {.08f, .15f, .25f, 1};
	if (bInMasked)
	{
		FModelImage Image{"Coverage", 2, 1};
		Image.Rgba = {255, 255, 255, 255, 255, 255, 255, 0};
		Source->Images.push_back(Image);
		Material.AlphaMode = EAlphaMode::Mask;
		Material.BaseColorTexture = {0, -1, 0};
	}
	Source->Materials.push_back(Material);
	Source->Primitives.push_back(Primitive);
	FModelNode Node;
	Node.Primitives = {0};
	if (bInSplit)
	{
		Source->Primitives[0].Indices = {0, 1, 2};
		Primitive.Indices = {0, 2, 3};
		Source->Primitives.push_back(Primitive);
		Node.Primitives.push_back(1);
	}
	Source->Nodes.push_back(Node);
	Source->Roots = {0};
	return PrepareSourceModel(Source);
}

struct FFixture
{
	FTaskSystem Tasks{2, 2};
	FWindow Window{"Selection outline regression", {320, 240}, true};
	FShaderCompiler Compiler{TestShaderRoot(), "outline-cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	FScene Scene;
	std::unique_ptr<FSceneRenderBridge> Bridge;
	std::unique_ptr<FSceneRenderPipeline> Pipeline;
	FSceneViewRequest View;
	FForwardPipelineStatistics Statistics;
	FScenePipelineSettings Settings;

	FFixture()
	{
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain({Surface, {320, 240}, ERHIDepthFormat::D32});
		                          }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
		Bridge = std::make_unique<FSceneRenderBridge>(Scene, *Session, Tasks);
		FRenderFeatureList Features;
		Features.push_back(MakeSelectionOutlineFeature(Device->GetCapabilities()));
		Pipeline =
		    std::make_unique<FSceneRenderPipeline>(*Session, Device->GetCapabilities(), Settings, std::move(Features));
		View.Width = 320;
		View.Height = 240;
		View.CameraOverride = FSceneCameraView{FSceneCamera{}, SceneCameraTransform({0, 0, 5}, {})};
	}

	~FFixture()
	{
		Pipeline.reset();
		Bridge.reset();
		Session->Close();
		Session.reset();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device->WaitIdle();
			                          Swapchain.reset();
			                          Device.reset();
		                          }));
	}

	FSceneHandle Add(std::shared_ptr<const FSceneModelData> InData, FMat4 InWorld = Identity())
	{
		FSceneModel Model;
		Model.Name = "Outline fixture";
		Model.Data = std::move(InData);
		Model.World = InWorld;
		return Scene.Add(std::move(Model));
	}

	FImage Frame(std::span<const FSceneHandle> InObjects, EOutlineOverlapMode InMode = EOutlineOverlapMode::Union,
	             bool bInSmooth = false, bool bInStale = false)
	{
		Window.Poll();
		Bridge->Flush();
		auto Outline = std::make_shared<FSelectionOutlineRequest>();
		Outline->Publication = Bridge->GetToken();
		if (bInStale)
		{
			++Outline->Publication->AttachmentEpoch;
		}
		Outline->Settings.Overlap = InMode;
		Outline->Settings.bSupersample = bInSmooth;
		for (const auto Object : InObjects)
		{
			Outline->Objects.push_back(Bridge->ResolveRenderPrimitives(Object));
		}
		const auto Seed = Session->FreezeSceneFrame(Bridge->GetToken());
		FImage Image;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          Pipeline->Configure(Settings);
			                          Pipeline->SetSelectionOutline(Outline);
			                          FRenderGraph Graph;
			                          FCascadedShadowSettings Shadows;
			                          Shadows.bEnabled = false;
			                          Pipeline->Build(Graph, View, Seed, Shadows, {.04f, .04f, .04f, 1}, {}, true);
			                          Image = ExecuteGraph(std::move(Graph), Tasks, *Swapchain,
			                                               {View.Width, View.Height}, false, true);
			                          Statistics = Pipeline->GetFrame().Statistics();
		                          }));
		return Image;
	}

	FImage Ready(std::span<const FSceneHandle> InObjects, EOutlineOverlapMode InMode = EOutlineOverlapMode::Union,
	             bool bInSmooth = false)
	{
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		FImage Image;
		do
		{
			Image = Frame(InObjects, InMode, bInSmooth);
			if (Statistics.SelectionOutline.Items && !Statistics.SelectionOutline.PendingItems)
			{
				return Image;
			}
		} while (std::chrono::steady_clock::now() < Deadline);
		throw std::runtime_error("Outline material did not become ready");
	}
};

bool IsOrange(const FImage& InImage, std::size_t InIndex)
{
	return InImage.Rgba[InIndex] > 220.0f / 255 && InImage.Rgba[InIndex + 1] > 100.0f / 255 &&
	       InImage.Rgba[InIndex + 1] < 210.0f / 255 && InImage.Rgba[InIndex + 2] < 95.0f / 255;
}

bool SameOutline(const FImage& InA, const FImage& InB)
{
	if (InA.Width != InB.Width || InA.Height != InB.Height)
	{
		return false;
	}
	for (std::size_t Index = 0; Index < InA.Rgba.size(); Index += 4)
	{
		if (IsOrange(InA, Index) != IsOrange(InB, Index))
		{
			return false;
		}
	}
	return true;
}

std::size_t Orange(const FImage& InImage, unsigned InLeft = 0, unsigned InTop = 0, unsigned InRight = 0,
                   unsigned InBottom = 0)
{
	std::size_t Count{};
	for (unsigned Y = InTop; Y < (InBottom ? InBottom : InImage.Height); ++Y)
	{
		for (unsigned X = InLeft; X < (InRight ? InRight : InImage.Width); ++X)
		{
			const auto Index = (std::size_t(Y) * InImage.Width + X) * 4;
			Count += IsOrange(InImage, Index);
		}
	}
	return Count;
}

void TestOverlap(FFixture& InFixture)
{
	const auto Data = Quad(false, true);
	const auto A = InFixture.Add(Data, Translation({-.4f, 0, 0}));
	const auto B = InFixture.Add(Data, Translation({.4f, 0, -.2f}));
	const std::array Objects{A, B};
	const auto Union = InFixture.Ready(Objects);
	HYP_CHECK(InFixture.Statistics.SelectionOutline.MaskPasses == 1);
	HYP_CHECK(Orange(Union) > 100);
	HYP_CHECK(Orange(Union, 150, 110, 170, 130) == 0);
	const auto PerObject = InFixture.Ready(Objects, EOutlineOverlapMode::PerObject);
	HYP_CHECK(InFixture.Statistics.SelectionOutline.MaskPasses == 2);
	HYP_CHECK(Orange(PerObject) > Orange(Union) + 100);
	HYP_CHECK(SameOutline(InFixture.Frame(Objects), Union));
	std::filesystem::create_directories("outline-results");
	SaveImage("outline-results/Union.png", Union);
	SaveImage("outline-results/PerObject.png", PerObject);
	const auto Wall = InFixture.Add(Quad(), Multiply(Translation({0, 0, 1}), Scale({3, 3, 1})));
	const auto Occluded = InFixture.Ready(Objects);
	HYP_CHECK(Orange(Occluded) == Orange(Union));
	SaveImage("outline-results/Occluded.png", Occluded);
	const auto Smooth = InFixture.Ready(Objects, EOutlineOverlapMode::Union, true);
	HYP_CHECK(Orange(Smooth) > 100);
	SaveImage("outline-results/Smooth.png", Smooth);
	HYP_CHECK(Orange(InFixture.Frame({})) == 0);
	HYP_CHECK(InFixture.Statistics.SelectionOutline.MaskPasses == 0);
	HYP_CHECK(Orange(InFixture.Frame(Objects, EOutlineOverlapMode::Union, false, true)) == 0);
	HYP_CHECK(InFixture.Statistics.SelectionOutline.bRejectedPublication);
	InFixture.Scene.Remove(Wall);
	InFixture.Scene.Remove(A);
	InFixture.Scene.Remove(B);
}

void TestCoverage(FFixture& InFixture)
{
	const auto Masked = InFixture.Add(Quad(true));
	const std::array Objects{Masked};
	const auto Image = InFixture.Ready(Objects);
	SaveImage("outline-results/Masked.png", Image);
	HYP_CHECK(Orange(Image) > 100);
	HYP_CHECK(Orange(Image, 205, 60, 225, 180) == 0);
	HYP_CHECK(Orange(Image, 155, 65, 166, 175) > 60);
	SaveImage("outline-results/Masked.png", Image);
	InFixture.Settings.Exposure = 5;
	const auto Exposed = InFixture.Frame(Objects);
	HYP_CHECK(Orange(Exposed) == Orange(Image));
	InFixture.View.DepthConvention = EDepthConvention::Reversed;
	InFixture.Settings.Pipeline = ESceneRenderPipeline::Forward;
	HYP_CHECK(Orange(InFixture.Frame(Objects)) == Orange(Image));
	auto Model = *InFixture.Scene.Find(Masked);
	Model.Surface.Overrides = {{"Pbr.BaseColorFactor", FMaterialValue::Float(FVec4{1, 1, 1, 0})}};
	InFixture.Scene.Update(Masked, Model);
	HYP_CHECK(Orange(InFixture.Ready(Objects)) == 0);
	Model.Surface.Overrides.clear();
	Model.SectionSurfaces[0].Overrides = {{"Pbr.AlphaCutoff", FMaterialValue::Float(1.1f)}};
	InFixture.Scene.Update(Masked, Model);
	HYP_CHECK(Orange(InFixture.Ready(Objects)) == 0);
	Model.SectionSurfaces.clear();
	Model.World = Scale({-1, 1, 1});
	InFixture.Scene.Update(Masked, Model);
	HYP_CHECK(Orange(InFixture.Ready(Objects)) > 100);
	Model.World = Scale({0, 0, 0});
	InFixture.Scene.Update(Masked, Model);
	HYP_CHECK(Orange(InFixture.Frame(Objects)) == 0);
	InFixture.Scene.Remove(Masked);
	HYP_CHECK(Orange(InFixture.Frame(Objects)) == 0);
}

std::shared_ptr<const FMaterialDefinition> CustomMaskDefinition()
{
	std::ofstream Shader(TestShaderRoot() / "CustomOutline.hlsl");
	Shader << "#define HYP_MATERIAL_VIEW_V1\n#define HYP_MATERIAL_OBJECT_V1\n"
	          "#include \"MaterialBlocks.hlsli\"\n"
	          R"SHADER(cbuffer CustomV1 : register(b4)
{
	float Cutoff;
	float4 ColorOnly;
};
struct FOutput
{
	float4 Position : SV_Position;
	float2 Uv : TEXCOORD0;
};
FOutput VSMain(float3 InPosition : POSITION, float2 InUv : TEXCOORD0)
{
	FOutput Result;
	Result.Position = mul(ViewProjection, mul(World, float4(InPosition, 1)));
	Result.Uv = InUv;
	return Result;
}
float4 PSMain(FOutput InInput) : SV_Target0
{
	clip(InInput.Uv.x - Cutoff);
	return ColorOnly;
}
float PSMask(FOutput InInput) : SV_Target0
{
	clip(InInput.Uv.x - Cutoff);
	return 1;
}
)SHADER";
	FMaterialDescription Description;
	Description.Name = "Reflected custom outline";
	FMaterialPass Pass;
	Pass.Vertex = {"CustomOutline.hlsl", "VSMain"};
	Pass.Pixel = {"CustomOutline.hlsl", "PSMain"};
	Pass.State.Cull = EMaterialCull::None;
	Description.Passes.push_back(Pass);
	Pass.Usage = "SilhouetteMask";
	Pass.Pixel.Entry = "PSMask";
	Description.Passes.push_back(Pass);
	return std::make_shared<const FMaterialDefinition>(std::move(Description));
}

void TestCustomCoverage(FFixture& InFixture)
{
	const auto Definition = CustomMaskDefinition();
	FCompiledMaterialDefinition Compiled;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Worker},
	                                              [&]
	                                              {
		                                              Compiled = CompileMaterialDefinition(
		                                                  InFixture.Compiler, Definition,
		                                                  InFixture.Device->GetCapabilities().ShaderFormat);
	                                              }));
	HYP_CHECK(Definition->GetSchema()->GetParameters().empty());
	FMaterialInstance Instance(Compiled.Interface);
	Instance.Set("Cutoff", FMaterialValue::Float(.5f));
	Instance.Set("ColorOnly", FMaterialValue::Float(FVec4{.1f, .1f, .1f, 1}));
	const auto Object = InFixture.Add(Quad());
	const std::array Objects{Object};
	auto Model = *InFixture.Scene.Find(Object);
	Model.Surface.Snapshot = Instance.Freeze();
	InFixture.Scene.Update(Object, Model);
	const auto Image = InFixture.Ready(Objects);
	HYP_CHECK(Orange(Image) > 100);
	HYP_CHECK(Orange(Image, 95, 60, 115, 180) == 0);
	HYP_CHECK(Orange(Image, 155, 65, 166, 175) > 60);
	HYP_CHECK(InFixture.Statistics.SelectionOutline.UnsupportedItems == 0);
	Instance.Set("Cutoff", FMaterialValue::Float(1.1f));
	Model.Surface.Snapshot = Instance.Freeze();
	InFixture.Scene.Update(Object, Model);
	HYP_CHECK(Orange(InFixture.Ready(Objects)) == 0);
	// A declared snapshot can carry reflected object overrides once its source interface is ready.
	Model.Surface.Snapshot = FMaterialInstance(Definition).Freeze();
	Model.Surface.Overrides = {{"Pixel:CustomV1.Cutoff", FMaterialValue::Float(.5f)},
	                           {"Pixel:CustomV1.ColorOnly", FMaterialValue::Float(FVec4{.1f, .1f, .1f, 1})}};
	InFixture.Scene.Update(Object, Model);
	HYP_CHECK(SameOutline(InFixture.Ready(Objects), Image));
	InFixture.Scene.Remove(Object);
}

void TestLifecycle(FFixture& InFixture)
{
	const auto Outer = InFixture.Add(Quad());
	const std::array Single{Outer};
	const auto Baseline = InFixture.Ready(Single);
	const auto Handles = InFixture.Bridge->ResolveRenderPrimitives(Outer);
	HYP_CHECK(!Handles.empty());
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderView View;
		    auto Duplicates = Handles;
		    Duplicates.insert(Duplicates.end(), Handles.begin(), Handles.end());
		    HYP_CHECK(InFixture.Session->GetScene().CollectPrimitives(View, Duplicates).Items.Size() == Handles.size());
		    ++Duplicates.front().Generation;
		    Duplicates.resize(1);
		    HYP_CHECK(InFixture.Session->GetScene().CollectPrimitives(View, Duplicates).Items.IsEmpty());
	    }));
	const auto Inner = InFixture.Add(Quad(), Scale({.4f, .4f, 1}));
	const std::array Pair{Outer, Inner};
	HYP_CHECK(Orange(InFixture.Ready(Pair)) == Orange(Baseline));
	HYP_CHECK(Orange(InFixture.Ready(Pair, EOutlineOverlapMode::PerObject)) > Orange(Baseline));
	const std::array Duplicates{Outer, Outer};
	HYP_CHECK(Orange(InFixture.Ready(Duplicates, EOutlineOverlapMode::PerObject)) == Orange(Baseline));
	HYP_CHECK(SameOutline(InFixture.Frame(Single), Baseline));
	InFixture.Scene.SetEnabled(Outer, false);
	HYP_CHECK(Orange(InFixture.Frame(Single)) == 0);
	InFixture.Scene.SetEnabled(Outer, true);
	InFixture.View.Width = 400;
	InFixture.View.Height = 280;
	const auto Resized = InFixture.Ready(Single, EOutlineOverlapMode::Union, true);
	HYP_CHECK(Resized.Width == 400 && Resized.Height == 280 && Orange(Resized) > 100);
	const auto Camera = InFixture.View.CameraOverride;
	InFixture.View.CameraOverride.reset();
	InFixture.View.bAllowCameraFallback = false;
	HYP_CHECK(Orange(InFixture.Frame(Single)) == 0);
	HYP_CHECK(InFixture.Statistics.SelectionOutline.MaskPasses == 0);
	InFixture.View.CameraOverride = Camera;
	HYP_CHECK(Orange(InFixture.Ready(Single)) > 100);
	auto Unsupported = *InFixture.Scene.Find(Outer);
	Unsupported.Surface.Snapshot = MakeGeometryPreviewMaterial();
	InFixture.Scene.Update(Outer, Unsupported);
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
	do
	{
		HYP_CHECK(Orange(InFixture.Frame(Single)) == 0);
	} while (InFixture.Statistics.SelectionOutline.PendingItems && std::chrono::steady_clock::now() < Deadline);
	HYP_CHECK(InFixture.Statistics.SelectionOutline.PendingItems == 0);
	HYP_CHECK(InFixture.Statistics.SelectionOutline.UnsupportedItems == 1);
	Unsupported.Surface = {};
	InFixture.Scene.Update(Outer, Unsupported);
	HYP_CHECK(Orange(InFixture.Ready(Single)) > 100);
	InFixture.Pipeline = std::make_unique<FSceneRenderPipeline>(*InFixture.Session, InFixture.Device->GetCapabilities(),
	                                                            InFixture.Settings, FRenderFeatureList{});
	HYP_CHECK(Orange(InFixture.Frame(Single)) == 0);
	HYP_CHECK(InFixture.Statistics.SelectionOutline.MaskPasses == 0);
	InFixture.Scene.Remove(Inner);
	InFixture.Scene.Remove(Outer);
	HYP_CHECK(InFixture.Bridge->ResolveRenderPrimitives(Outer).empty());
}
} // namespace

int main()
{
	try
	{
		FFixture Fixture;
		TestOverlap(Fixture);
		TestCoverage(Fixture);
		TestCustomCoverage(Fixture);
		TestLifecycle(Fixture);
		Fixture.Tasks.Wait(Fixture.Tasks.Dispatch({EDomain::Rhi, 0},
		                                          [&]
		                                          {
			                                          Fixture.Device->WaitIdle();
			                                          HYP_CHECK(Fixture.Device->Statistics().ValidationErrors == 0);
		                                          }));
		std::cout << "Selection outline tests passed; GPU validation errors=0\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
