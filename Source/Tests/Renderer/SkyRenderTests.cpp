#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Support/ModelAssetSupport.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace Hyperion;

namespace
{
struct FSkyFixture
{
	FTaskSystem Tasks{2, 1};
	FIOService IO{Tasks};
	FAssetService Assets{IO};
	FWindow Window{"Sky rendering", {256, 192}, true};
	FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "sky-test-cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	std::unique_ptr<FSceneInstance> Scene;
	std::unique_ptr<FSceneRenderPipeline> Pipeline;
	FSceneHandle Camera;
	FSceneHandle Environment;
	FScenePipelineSettings Settings;
	FForwardPipelineStatistics Statistics;
	EDepthConvention Convention;

	explicit FSkyFixture(EDepthConvention InConvention) : Convention(InConvention)
	{
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain(
			                              {Surface, {256, 192}, ERHIDepthFormat::D32, GetDepthClearValue(Convention)});
		                          }));
		RegisterSceneAssetTypes(Assets.Types());
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
		Scene = std::make_unique<FSceneInstance>(*Session, Tasks, Assets);
		Pipeline = std::make_unique<FSceneRenderPipeline>(*Session, Device->GetCapabilities());
		auto Node = MakeSceneCameraNode("camera");
		Node.Local = Translation({0, 0, 4});
		Camera = Scene->AddNode(Node);
		Environment = Scene->AddNode(MakeSceneEnvironmentLightNode("environment"));
		Scene->SetSettings({Camera, {}, Environment});
	}

	~FSkyFixture()
	{
		Scene->Close();
		Scene.reset();
		Pipeline.reset();
		Session->Close();
		Session.reset();
		Assets.Drain();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device->WaitIdle();
			                          Swapchain.reset();
			                          Device.reset();
		                          }));
	}

	template<class T> FAssetRef Write(const std::string& InName, const T& InValue)
	{
		const auto Path = std::filesystem::absolute("sky-test-data/" + InName + ".hasset");
		auto Encoded = EncodeAsset(RecordType<T>(), &InValue);
		IO.WriteAsync(Path, std::move(Encoded.Bytes)).Get(Tasks);
		return {Encoded.Header.Id, PathToUtf8(Path), Encoded.Header.TypeId, Encoded.Header.Revision};
	}

	FAssetRef MakeSky(std::string InName, FVec3 InColor)
	{
		std::vector<float> Pixels(16 * 8 * 4, 1);
		for (std::size_t Index = 0; Index < Pixels.size(); Index += 4)
		{
			Pixels[Index] = InColor.X;
			Pixels[Index + 1] = InColor.Y;
			Pixels[Index + 2] = InColor.Z;
		}
		auto Bake = BakeEnvironment(Pixels, 16, 8, {8, 4, 64});
		if (InName == "Blue")
		{
			for (auto* Texture : {&Bake.Radiance, &Bake.Specular})
			{
				for (auto& Mip : Texture->Mips)
				{
					FMaterialTextureMip Wide{Mip.Width, Mip.Height, std::vector<std::uint8_t>(Mip.Bytes.size() * 2)};
					for (std::size_t Pixel = 0; Pixel < Mip.Bytes.size() / 8; ++Pixel)
					{
						WriteTexturePixel(Wide, ETextureFormat::Rgba32Float, Pixel,
						                  ReadTexturePixel(Mip, Texture->Format, Pixel));
					}
					Mip = std::move(Wide);
				}
				Texture->Format = ETextureFormat::Rgba32Float;
			}
		}
		FSkyAsset Sky;
		Sky.Name = InName;
		Sky.Irradiance = Bake.Irradiance;
		Sky.Radiance = Write(InName + "Radiance", Bake.Radiance);
		Sky.Specular = Write(InName + "Specular", Bake.Specular);
		Sky.Brdf = Write(InName + "Brdf", BuildEnvironmentBrdf(32, 128));
		return Write(InName, Sky);
	}

	void Select(FAssetRef InReference)
	{
		auto Light = *Scene->FindNode(Environment)->EnvironmentLight;
		Light.Source = ESceneEnvironmentSource::SkyAsset;
		Light.Sky = std::move(InReference);
		Scene->SetEnvironmentLight(Environment, Light);
	}

	void Await(bool bInFailure = false)
	{
		const auto End = std::chrono::steady_clock::now() + std::chrono::seconds(20);
		do
		{
			Scene->Tick();
			HYP_CHECK(Scene->GetStatus().Error.empty());
			if (bInFailure ? Scene->GetStatus().FailedSkies > 0 : Scene->GetStatus().bReady)
			{
				Tasks.Wait(Scene->GetReceipt());
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		} while (std::chrono::steady_clock::now() < End);
		throw std::runtime_error("Scene did not become ready: " + Scene->GetSkyStatus(Environment));
	}

	FImage Frame(std::optional<FViewport> InViewport = {})
	{
		Window.Poll();
		Scene->Tick();
		Tasks.Wait(Scene->GetReceipt());
		const auto Seed = Session->FreezeSceneFrame(Scene->GetToken());
		FImage Image;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          Pipeline->Configure(Settings);
			                          FRenderGraph Graph;
			                          FSceneViewRequest Request;
			                          Request.Width = 256;
			                          Request.Height = 192;
			                          Request.DepthConvention = Convention;
			                          Request.Viewport = InViewport;
			                          FCascadedShadowSettings Shadows;
			                          Shadows.bEnabled = false;
			                          Pipeline->Build(Graph, Request, Seed, Shadows, {0, 0, 0, 1}, {}, true);
			                          const auto Frame = Pipeline->GetFrame();
			                          Image =
			                              ExecuteGraph(std::move(Graph), Tasks, *Swapchain, {256, 192}, false, true);
			                          Statistics = Frame.Statistics();
			                          for (const auto& Row : Frame.Statistics().Views)
			                          {
				                          HYP_CHECK(Row.Visibility.Batches.FailedItems == 0);
			                          }
		                          }));
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          HYP_CHECK(Device->Statistics().ValidationErrors == 0);
		                          }));
		return Image;
	}
};

float Pixel(const FImage& InImage, unsigned InX, unsigned InY, unsigned InChannel = 0)
{
	return InImage.Rgba.at((std::size_t(InY) * InImage.Width + InX) * 4 + InChannel);
}

void Similar(const FImage& InA, const FImage& InB, float InTolerance = .02f)
{
	float Maximum{};
	for (std::size_t Index = 0; Index < InA.Rgba.size(); ++Index)
	{
		Maximum = std::max(Maximum, std::abs(InA.Rgba[Index] - InB.Rgba[Index]));
	}
	std::cout << "sky image max delta=" << Maximum << '\n';
	HYP_CHECK(Maximum < InTolerance);
}

std::shared_ptr<const FSceneModelData> Quad(float InMetallic, EAlphaMode InAlpha = EAlphaMode::Opaque,
                                            float InRoughness = .35f)
{
	auto Model = std::make_shared<FModelSource>();
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
	Primitive.Normals = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
	Primitive.Indices = {0, 1, 2, 0, 2, 3};
	Primitive.Material = 0;
	Model->Primitives.push_back(Primitive);
	FModelMaterial Material;
	Material.BaseColor = {.5f, .5f, .5f, InAlpha == EAlphaMode::Blend ? .5f : 1.f};
	Material.Metallic = InMetallic;
	Material.Roughness = InRoughness;
	Material.AlphaMode = InAlpha;
	Model->Materials.push_back(Material);
	FModelNode Node;
	Node.Primitives = {0};
	Model->Nodes.push_back(Node);
	Model->Roots = {0};
	return PrepareSourceModel(Model);
}

void CheckDirections(FSkyFixture& InFixture)
{
	FTextureAsset Cube;
	Cube.Dimension = ETextureDimension::Cube;
	Cube.Format = ETextureFormat::Rgba16Float;
	const std::array<FVec3, 6> Axes{{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
	const std::array<FVec3, 6> Colors{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {1, 0, 1}, {0, 1, 1}}};
	for (unsigned Size : {4, 2, 1})
	{
		FMaterialTextureMip Mip{Size, Size, std::vector<std::uint8_t>(Size * Size * 6 * 8)};
		for (unsigned Face = 0; Face < 6; ++Face)
		{
			for (unsigned Pixel = 0; Pixel < Size * Size; ++Pixel)
			{
				const auto Color = Colors[Face];
				WriteTexturePixel(Mip, Cube.Format, Face * Size * Size + Pixel, {Color.X, Color.Y, Color.Z, 1});
			}
		}
		Cube.Mips.push_back(std::move(Mip));
	}
	FSkyAsset Sky;
	Sky.Radiance = InFixture.Write("DirectionsCube", Cube);
	Sky.Specular = Sky.Radiance;
	Sky.Brdf = InFixture.Write("DirectionsBrdf", BuildEnvironmentBrdf(8, 32));
	Sky.Irradiance = ProjectEnvironmentSh(Cube);
	InFixture.Select(InFixture.Write("Directions", Sky));
	InFixture.Await();
	for (unsigned Face = 0; Face < 6; ++Face)
	{
		const auto Up = std::abs(Axes[Face].Y) > .9f ? FVec3{0, 0, 1} : FVec3{0, 1, 0};
		InFixture.Scene->SetLocalTransform(InFixture.Camera, SceneCameraTransform({}, Axes[Face], Up));
		const auto Image = InFixture.Frame();
		const std::array Expected{Colors[Face].X, Colors[Face].Y, Colors[Face].Z};
		for (unsigned Channel = 0; Channel < 3; ++Channel)
		{
			HYP_CHECK(Expected[Channel] > .5f ? Pixel(Image, 128, 96, Channel) > .6f
			                                  : Pixel(Image, 128, 96, Channel) < .03f);
		}
	}
	InFixture.Scene->SetLocalTransform(InFixture.Camera, SceneCameraTransform({}, {0, 0, 1}));
	auto Light = *InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight;
	Light.YawRadians = 1.570796327f;
	InFixture.Scene->SetEnvironmentLight(InFixture.Environment, Light);
	const auto Rotated = InFixture.Frame();
	HYP_CHECK(Pixel(Rotated, 128, 96, 1) > .6f && Pixel(Rotated, 128, 96, 0) < .03f);
	Light.YawRadians = 0;
	InFixture.Scene->SetEnvironmentLight(InFixture.Environment, Light);
	InFixture.Scene->SetLocalTransform(InFixture.Camera, Translation({0, 0, 4}));
}

void CheckCameraTranslation(FSkyFixture& InFixture)
{
	FTextureAsset Cube;
	Cube.Dimension = ETextureDimension::Cube;
	Cube.Format = ETextureFormat::Rgba16Float;
	for (unsigned Size : {16, 8, 4, 2, 1})
	{
		FMaterialTextureMip Mip{Size, Size, std::vector<std::uint8_t>(Size * Size * 6 * 8)};
		for (unsigned Face = 0; Face < 6; ++Face)
		{
			for (unsigned Y = 0; Y < Size; ++Y)
			{
				for (unsigned X = 0; X < Size; ++X)
				{
					const auto D = CubeDirection(Face, (X + .5f) * 2 / Size - 1, (Y + .5f) * 2 / Size - 1);
					WriteTexturePixel(Mip, Cube.Format, (Face * Size + Y) * Size + X,
					                  {.5f + .5f * D.X, .5f + .5f * D.Y, .5f + .5f * D.Z, 1});
				}
			}
		}
		Cube.Mips.push_back(std::move(Mip));
	}
	FSkyAsset Sky;
	Sky.Radiance = InFixture.Write("TranslationCube", Cube);
	Sky.Specular = Sky.Radiance;
	Sky.Brdf = InFixture.Write("TranslationBrdf", BuildEnvironmentBrdf(8, 16));
	InFixture.Select(InFixture.Write("Translation", Sky));
	InFixture.Await();
	const FViewport Viewport{32, 24, 192, 144, .2f, .8f};
	for (const auto Pipeline : {ESceneRenderPipeline::Forward, ESceneRenderPipeline::Deferred})
	{
		InFixture.Settings.Pipeline = Pipeline;
		for (const auto Forward : {FVec3{0, 0, -1}, FVec3{.2f, .3f, -1}})
		{
			const auto Rotation = SceneCameraTransform({}, Forward, {0, 1, .2f});
			InFixture.Scene->SetLocalTransform(InFixture.Camera, Rotation);
			const auto Base = InFixture.Frame(Viewport);
			for (const float Distance : {1000.f, 10000.f, 100000.f, 1000000.f})
			{
				InFixture.Scene->SetLocalTransform(
				    InFixture.Camera, Multiply(Translation({Distance, -Distance * .3f, Distance * .2f}), Rotation));
				Similar(Base, InFixture.Frame(Viewport), .000001f);
			}
		}
	}
	InFixture.Scene->SetLocalTransform(InFixture.Camera, Translation({0, 0, 4}));
	std::cout << "Directional sky is invariant under large camera translations\n";
}

void CheckSourceToggle(FSkyFixture& InFixture, const FAssetRef& InReference)
{
	InFixture.Select(InReference);
	InFixture.Await();
	const auto Active = *InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight;
	const auto Before = InFixture.Frame();
	for (const bool bSeparateTicks : {false, true})
	{
		auto Constant = Active;
		Constant.Source = ESceneEnvironmentSource::ConstantColor;
		InFixture.Scene->SetEnvironmentLight(InFixture.Environment, Constant);
		if (bSeparateTicks)
		{
			InFixture.Scene->Tick();
		}
		InFixture.Scene->SetEnvironmentLight(InFixture.Environment, Active);
		InFixture.Scene->Tick();
		HYP_CHECK(!InFixture.Scene->GetStatus().bReady ||
		          InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight->Data);
		InFixture.Await();
		const auto Data = InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight->Data;
		HYP_CHECK(Data && Data->Reference == Active.Data->Reference);
		Similar(Before, InFixture.Frame());
	}
	std::cout << "Sky source transitions retain readiness/data consistency\n";
}

void CheckFailedRetry(FSkyFixture& InFixture, const FAssetRef& InReference, bool bInWrongType)
{
	const auto Previous = InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight->Data;
	const auto Name = "Retry" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	const FAssetRef Missing{"", PathToUtf8(std::filesystem::absolute("sky-test-data/" + Name + ".hasset")),
	                        RecordType<FSkyAsset>().Id, ""};
	if (bInWrongType)
	{
		InFixture.Write(Name, BuildEnvironmentBrdf(8, 16));
	}
	InFixture.Select(Missing);
	InFixture.Await(true);
	HYP_CHECK(InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight->Data == Previous);
	const auto Loaded = InFixture.Assets.LoadReferenceAsync(InReference, {}).Get(InFixture.Tasks);
	auto Sky = *Loaded->As<FSkyAsset>();
	Sky.Name = Name;
	const auto Repaired = InFixture.Write(Name, Sky);
	InFixture.Select(Missing);
	HYP_CHECK(InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight->Data == Previous);
	InFixture.Await();
	const auto Data = InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight->Data;
	HYP_CHECK(Data && Data->Reference.Id == Repaired.Id && Data->Reference.Revision == Repaired.Revision);
	HYP_CHECK(InFixture.Scene->GetStatus().FailedSkies == 0);
	std::cout << "Applying the same failed sky path retries and publishes the repaired file (wrong type="
	          << bInWrongType << ")\n";
}

void CheckPixels(FSkyFixture& InFixture, const FAssetRef& InRed)
{
	InFixture.Select(InRed);
	InFixture.Await();
	const auto Background = InFixture.Frame();
	HYP_CHECK(Pixel(Background, 128, 96) > .8f && Pixel(Background, 128, 96, 2) < .45f);
	InFixture.Settings.Pipeline = ESceneRenderPipeline::Forward;
	Similar(Background, InFixture.Frame());
	InFixture.Scene->SetLocalTransform(InFixture.Camera, Translation({5, 2, 4}));
	Similar(Background, InFixture.Frame());
	InFixture.Scene->SetLocalTransform(InFixture.Camera, Translation({0, 0, 4}));
	const auto Subview = InFixture.Frame(FViewport{32, 24, 192, 144, .2f, .8f});
	HYP_CHECK(Pixel(Subview, 4, 4) < .01f && Pixel(Subview, 128, 96) > .8f);
	for (const auto Metallic : {0.f, 1.f})
	{
		const auto Handle = InFixture.Scene->Add({"Surface", Quad(Metallic)});
		InFixture.Await();
		InFixture.Settings.Pipeline = ESceneRenderPipeline::Deferred;
		const auto Deferred = InFixture.Frame();
		HYP_CHECK(Pixel(Deferred, 128, 96) > .25f && Pixel(Deferred, 128, 96) < Pixel(Background, 128, 96) - .05f);
		HYP_CHECK(Pixel(Deferred, 128, 96) > Pixel(Deferred, 128, 96, 2) + .15f);
		InFixture.Settings.Pipeline = ESceneRenderPipeline::Forward;
		Similar(Deferred, InFixture.Frame());
		auto Light = *InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight;
		Light.bVisible = false;
		InFixture.Scene->SetEnvironmentLight(InFixture.Environment, Light);
		const auto Hidden = InFixture.Frame();
		HYP_CHECK(Pixel(Hidden, 4, 4) < .01f);
		HYP_CHECK(std::abs(Pixel(Hidden, 128, 96) - Pixel(Deferred, 128, 96)) < .02f);
		Light.bVisible = true;
		InFixture.Scene->SetEnvironmentLight(InFixture.Environment, Light);
		InFixture.Scene->Remove(Handle);
	}
	const auto Transparent = InFixture.Scene->Add({"Transparent", Quad(0, EAlphaMode::Blend)});
	InFixture.Await();
	InFixture.Settings.Pipeline = ESceneRenderPipeline::Deferred;
	const auto Blended = InFixture.Frame();
	HYP_CHECK(Pixel(Blended, 128, 96) < Pixel(Background, 128, 96) - .01f && Pixel(Blended, 128, 96) > .4f);
	InFixture.Settings.Pipeline = ESceneRenderPipeline::Forward;
	Similar(Blended, InFixture.Frame());
	InFixture.Scene->Remove(Transparent);
}

void CheckClustered(FSkyFixture& InFixture)
{
	const auto Surface = InFixture.Scene->Add({"Cluster surface", Quad(.5f)});
	auto Point = MakeScenePointLightNode("point");
	Point.Local = Translation({0, 1, 2});
	Point.PointLight->Intensity = 2;
	const auto PointHandle = InFixture.Scene->AddNode(Point);
	InFixture.Await();
	for (const bool bDirectional : {false, true})
	{
		FSceneHandle Sun;
		if (bDirectional)
		{
			Sun = InFixture.Scene->AddNode(MakeSceneDirectionalLightNode("sun"));
			InFixture.Scene->SetSettings({InFixture.Camera, Sun, InFixture.Environment});
		}
		InFixture.Settings.Pipeline = ESceneRenderPipeline::Deferred;
		InFixture.Settings.bClusteredLighting = true;
		const auto Clustered = InFixture.Frame();
		HYP_CHECK(InFixture.Statistics.LocalLights.ClusterReferences > 0);
		InFixture.Settings.bClusteredLighting = false;
		Similar(Clustered, InFixture.Frame());
		InFixture.Settings.bClusteredLighting = true;
		InFixture.Settings.Pipeline = ESceneRenderPipeline::Forward;
		Similar(Clustered, InFixture.Frame());
		if (bDirectional)
		{
			InFixture.Scene->RemoveSubtree(Sun);
			InFixture.Scene->SetSettings({InFixture.Camera, {}, InFixture.Environment});
		}
	}
	InFixture.Settings.bClusteredLighting = true;
	InFixture.Scene->RemoveSubtree(PointHandle);
	InFixture.Scene->Remove(Surface);
}

void CheckRoughness(FSkyFixture& InFixture)
{
	FTextureAsset Cube;
	Cube.Dimension = ETextureDimension::Cube;
	Cube.Format = ETextureFormat::Rgba16Float;
	for (unsigned Size : {4, 2, 1})
	{
		FMaterialTextureMip Mip{Size, Size, std::vector<std::uint8_t>(Size * Size * 6 * 8)};
		for (unsigned Pixel = 0; Pixel < Size * Size * 6; ++Pixel)
		{
			WriteTexturePixel(Mip, Cube.Format, Pixel,
			                  Size == 1 ? std::array<float, 4>{0, 0, 2, 1} : std::array<float, 4>{2, 0, 0, 1});
		}
		Cube.Mips.push_back(std::move(Mip));
	}
	FSkyAsset Sky;
	Sky.Radiance = InFixture.Write("RoughnessCube", Cube);
	Sky.Specular = Sky.Radiance;
	Sky.Brdf = InFixture.Write("RoughnessBrdf", BuildEnvironmentBrdf(32, 128));
	InFixture.Select(InFixture.Write("Roughness", Sky));
	InFixture.Await();
	for (const float Roughness : {.05f, 1.f})
	{
		const auto Model = InFixture.Scene->Add({"Roughness", Quad(1, EAlphaMode::Opaque, Roughness)});
		InFixture.Await();
		InFixture.Settings.Pipeline = ESceneRenderPipeline::Deferred;
		const auto Image = InFixture.Frame();
		const auto Dominant = Roughness < .5f ? 0 : 2;
		const auto Other = Roughness < .5f ? 2 : 0;
		HYP_CHECK(Pixel(Image, 128, 96, Dominant) > .3f && Pixel(Image, 128, 96, Other) < .04f);
		InFixture.Settings.Pipeline = ESceneRenderPipeline::Forward;
		Similar(Image, InFixture.Frame());
		InFixture.Scene->Remove(Model);
	}
}

void CheckReplacement(FSkyFixture& InFixture, const FAssetRef& InRed, const FAssetRef& InBlue)
{
	const auto Previous = InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight->Data;
	auto Missing = InBlue;
	Missing.Id.clear();
	Missing.Revision.clear();
	Missing.Path = PathToUtf8(std::filesystem::absolute("sky-test-data/Missing.hasset"));
	InFixture.Select(Missing);
	InFixture.Await(true);
	HYP_CHECK(InFixture.Scene->FindNode(InFixture.Environment)->EnvironmentLight->Data == Previous);
	const auto Destination = std::filesystem::absolute("sky-test-data/saved/Scene.hasset");
	const auto FailedSnapshot = InFixture.Scene->Snapshot(Destination);
	HYP_CHECK(FailedSnapshot.Nodes.back().EnvironmentLight->Sky->Path.find("Missing.hasset") != std::string::npos);
	InFixture.Select(InBlue);
	InFixture.Scene->Tick();
	InFixture.Select(InRed);
	InFixture.Scene->Tick();
	InFixture.Select(InBlue);
	InFixture.Await();
	const auto Blue = InFixture.Frame();
	HYP_CHECK(Pixel(Blue, 128, 96, 2) > Pixel(Blue, 128, 96) + .3f);
	auto Snapshot = InFixture.Scene->Snapshot(Destination);
	InFixture.IO.WriteAsync(Destination, EncodeAsset(RecordType<FSceneManifest>(), &Snapshot).Bytes)
	    .Get(InFixture.Tasks);
	InFixture.Scene->Load(Destination);
	InFixture.Camera = {};
	InFixture.Await();
	InFixture.Camera = InFixture.Scene->FindHandle("camera");
	InFixture.Environment = InFixture.Scene->FindHandle("environment");
	Similar(Blue, InFixture.Frame());
	const auto Uploads = InFixture.Session->GetResources().Statistics().Materials.TextureUploads;
	for (unsigned Frame = 0; Frame < 8; ++Frame)
	{
		InFixture.Scene->SetLocalTransform(InFixture.Camera, Translation({float(Frame) * .1f, 0, 4}));
		(void)InFixture.Frame();
	}
	HYP_CHECK(InFixture.Session->GetResources().Statistics().Materials.TextureUploads == Uploads);
}
} // namespace

int main()
{
	try
	{
		for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
		{
			FSkyFixture Fixture(Convention);
			const auto Red = Fixture.MakeSky("Red", {2, .15f, .05f});
			const auto Blue = Fixture.MakeSky("Blue", {.05f, .15f, 2});
			CheckDirections(Fixture);
			CheckCameraTranslation(Fixture);
			CheckPixels(Fixture, Red);
			CheckClustered(Fixture);
			CheckRoughness(Fixture);
			CheckReplacement(Fixture, Red, Blue);
			CheckSourceToggle(Fixture, Red);
			CheckFailedRetry(Fixture, Blue, false);
			CheckFailedRetry(Fixture, Blue, true);
			Fixture.Select(Red);
			Fixture.Scene->Tick();
			Fixture.Scene->Close();
		}
		std::cout << "Sky depth, IBL, transparency, switching, save/reload and stable uploads passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
