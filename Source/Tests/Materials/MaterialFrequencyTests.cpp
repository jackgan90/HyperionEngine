#include "Hyperion/Renderer/RenderSession.h"
#include "Support/TestSupport.h"
#include <bit>
#include <chrono>
#include <fstream>
#include <thread>

using namespace Hyperion;

namespace
{
constexpr std::size_t DrawCount = 24;

struct FFrequencyInputs
{
	float Object{};
	float Draw{};
	std::size_t Count = DrawCount;
	std::optional<std::uint64_t> FirstId = 0;
};

class FFrequencyPrimitive final : public IRenderPrimitive
{
public:
	FFrequencyPrimitive(FTaskSystem& InTasks, std::shared_ptr<FFrequencyInputs> InInputs)
	    : IRenderPrimitive(InTasks), Inputs(std::move(InInputs))
	{
	}

	void Collect(const FRenderView&, std::vector<FRenderItem>& OutItems) const override
	{
		for (std::size_t Index = 0; Index < Inputs->Count; ++Index)
		{
			FRenderItem Item;
			Item.State = GetState();
			Item.LocalItemId = Inputs->FirstId ? std::optional(*Inputs->FirstId + Index) : std::nullopt;
			Item.State.ObjectInputs = {{"Test.Object", FMaterialValue::Float(Inputs->Object + float(Index) * .001f)}};
			Item.DrawInputs = {{"Test.Draw", FMaterialValue::Float(Inputs->Draw + float(Index) * .001f)}};
			OutItems.push_back(std::move(Item));
		}
	}

private:
	std::shared_ptr<FFrequencyInputs> Inputs;
};

float Read(const FMaterialProviderInputs& InInputs, EMaterialScope InScope, std::string_view InName,
           std::size_t InWord = 0)
{
	const auto* Value = InInputs.Find(InScope, InName);
	if (!Value)
	{
		throw std::runtime_error("Missing frequency input");
	}
	return std::bit_cast<float>(Value->Words.at(InWord));
}

std::shared_ptr<const FMaterialSemanticRegistry> Semantics()
{
	auto Result = std::make_shared<FMaterialSemanticRegistry>();
	const std::array<std::pair<const char*, EMaterialScope>, 7> Entries{{{"Test.Global", EMaterialScope::Global},
	                                                                     {"Test.Frame", EMaterialScope::Frame},
	                                                                     {"Test.View", EMaterialScope::View},
	                                                                     {"Test.Object", EMaterialScope::Object},
	                                                                     {"Test.Pass", EMaterialScope::Pass},
	                                                                     {"Test.Draw", EMaterialScope::Draw},
	                                                                     {"Test.Mixed", EMaterialScope::View}}};
	for (const auto& [Name, Scope] : Entries)
	{
		Result->Register({Name, FMaterialParameterType::Numeric(EMaterialScalar::Float), Scope, "Frequency test"});
	}
	Result->Freeze();
	return Result;
}

std::filesystem::path WriteShader()
{
	const auto Root = std::filesystem::absolute("material-frequency-test/source");
	std::filesystem::create_directories(Root);
	std::ofstream(Root / "Frequency.hlsl") << R"(
cbuffer GlobalData : register(b0) { float GlobalGain; };
cbuffer FrameData : register(b1) { float FrameGain; };
cbuffer ViewData : register(b2) { float ViewGain; };
cbuffer ObjectData : register(b3) { float ObjectGain; };
cbuffer PassData : register(b4) { float PassGain; };
cbuffer DrawData : register(b5) { float DrawGain; };
cbuffer MixedData : register(b6) { float MixedGain; };
cbuffer MaterialData : register(b7) { float MaterialGain; };
float4 VSMain(float3 InPosition : POSITION) : SV_Position { return float4(InPosition, 1); }
float4 PSMain() : SV_Target0
{
    return float4(GlobalGain + FrameGain + ViewGain + ObjectGain + PassGain + DrawGain + MixedGain + MaterialGain, .1, .2, 1);
}
)";
	return Root;
}

std::shared_ptr<const FMaterialSnapshot> Surface(const std::shared_ptr<const FMaterialSemanticRegistry>& InSemantics)
{
	FMaterialDescription Description;
	Description.Name = "Independent frequency contract";
	FMaterialPass Pass;
	Pass.Vertex = {"Frequency.hlsl", "VSMain"};
	Pass.Pixel = {"Frequency.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	for (const std::string Name : {"Global", "Frame", "View", "Object", "Pass", "Draw", "Mixed"})
	{
		Description.Parameters.push_back(DeclareMaterialSemantic(Name + "Gain", "Test." + Name, *InSemantics));
	}
	FMaterialParameterDeclaration Material;
	Material.Name = "MaterialGain";
	Material.Default = FMaterialValue::Float(.02f);
	Description.Parameters.push_back(Material);
	return FMaterialInstance(std::make_shared<const FMaterialDefinition>(Description, InSemantics)).Freeze();
}

FRenderResourceDesc Geometry(const std::shared_ptr<const FMaterialSnapshot>& InSurface)
{
	FRenderResourceDesc Result;
	FRenderGeometryDesc Geometry;
	const std::array<FVec3, 3> Vertices{{{-1, -1, .5f}, {3, -1, .5f}, {-1, 3, .5f}}};
	const auto Bytes = std::as_bytes(std::span(Vertices));
	Geometry.Vertices.assign(Bytes.begin(), Bytes.end());
	Geometry.Indices = {0, 1, 2};
	Geometry.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0}};
	Geometry.VertexStride = sizeof(FVec3);
	Result.Geometries.push_back(std::move(Geometry));
	FRenderMaterialDesc Material;
	Material.Surface = InSurface;
	Result.Materials.push_back(std::move(Material));
	Result.Sections.push_back({0, 0, 0, 3});
	return Result;
}

struct FFrequencyFixture
{
	FTaskSystem Tasks{1, 1};
	std::shared_ptr<const FMaterialSemanticRegistry> Registry = Semantics();
	FShaderCompiler Compiler{WriteShader(), "material-frequency-test/cache"};
	FRenderSession Session;
	std::shared_ptr<FFrequencyInputs> Inputs = std::make_shared<FFrequencyInputs>();
	FRenderBinding Binding;
	FRenderPrimitiveState State;
	std::vector<FMaterialScopeKey> DrawKeys;
	FRenderView View;
	float Time{};
	float Pass{};

	explicit FFrequencyFixture(IRHIDevice& InDevice)
	    : Session(Tasks, InDevice, Compiler, ERHIDepthFormat::D32S8, Registry)
	{
		Session.GetProviders().Register(
		    {"Test.Draw", MaterialScopeBit(EMaterialScope::Draw), [this](const auto& InInputs)
		     {
			     DrawKeys.push_back(InInputs.Scopes[static_cast<std::size_t>(EMaterialScope::Draw)].Key);
			     return FMaterialValue::Float(Read(InInputs, EMaterialScope::Draw, "Test.Draw"));
		     }});
		Session.GetProviders().Register({"Test.Frame", MaterialScopeBit(EMaterialScope::Frame), [](const auto& InInputs)
		                                 {
			                                 return FMaterialValue::Float(
			                                     Read(InInputs, EMaterialScope::Frame, "Engine.Frame.Time"));
		                                 }});
		Session.GetProviders().Register(
		    {"Test.Mixed", MaterialScopeBit(EMaterialScope::Object) | MaterialScopeBit(EMaterialScope::View),
		     [](const auto& InInputs)
		     {
			     return FMaterialValue::Float(
			         Read(InInputs, EMaterialScope::Object, "Test.Object") +
			         Read(InInputs, EMaterialScope::View, "Test.View") +
			         Read(InInputs, EMaterialScope::Object, "Engine.Object.WorldViewProjection") * .01f);
		     }});
		const auto Material = Surface(Registry);
		State.Resource = Session.GetResources().Request(Material, 1, "frequency-test",
		                                                [Material]
		                                                {
			                                                return Geometry(Material);
		                                                });
		Binding = Session.GetScene().Create(State,
		                                    [Inputs = Inputs](FTaskSystem& InTasks)
		                                    {
			                                    return std::make_unique<FFrequencyPrimitive>(InTasks, Inputs);
		                                    });
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (Binding.GetStatus().State != ERenderPrimitiveStatus::Ready)
		{
			HYP_CHECK(std::chrono::steady_clock::now() < Deadline);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		View.Width = 64;
		View.Height = 64;
		View.CullingMode = ESceneCullingMode::None;
		View.Parameters = {{"Test.View", FMaterialValue::Float(.01f)}};
		Session.SetGlobalParameters({{"Test.Global", FMaterialValue::Float(.01f)}});
	}

	~FFrequencyFixture()
	{
		State = {};
		Session.Close();
	}

	std::vector<FPassCommands> Build(std::size_t InExpectedDraws = DrawCount)
	{
		View.PassParameters = {{"Test.Pass", FMaterialValue::Float(Pass)}};
		const auto Frame = Session.FreezeFrame(Time);
		std::vector<FPassCommands> Result;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          FRenderGraph Graph;
			                          FColorPass Clear;
			                          Clear.Commands.Name = "Frequency clear";
			                          Clear.Load = EColorLoad::Clear;
			                          Graph.Add(std::move(Clear));
			                          Session.BuildViews(Graph, std::span(&View, 1), Frame);
			                          Result = Graph.Compile();
		                          }));
		HYP_CHECK(Result.size() == 3 && Result[1].Draws.size() == InExpectedDraws);
		return Result;
	}

	float Render(IRHISwapchain& InSwapchain, const std::vector<FPassCommands>& InPasses)
	{
		FImage Image;
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          InSwapchain.BeginFrame({64, 64});
			                          std::vector<FRecordedList> Lists;
			                          for (std::uint32_t Index = 0; Index < InPasses.size(); ++Index)
			                          {
				                          Lists.push_back(InSwapchain.Record(Index, InPasses[Index]));
			                          }
			                          Image = InSwapchain.EndFrame(Lists, false, true);
		                          }));
		return Image.Rgba[(32 * 64 + 32) * 4];
	}
};

void CheckDrawIdentity(FFrequencyFixture& InFixture)
{
	InFixture.Inputs->Count = 1;
	auto OtherInputs = std::make_shared<FFrequencyInputs>(*InFixture.Inputs);
	auto Other =
	    InFixture.Session.GetScene().Create(InFixture.State,
	                                        [OtherInputs](FTaskSystem& InTasks)
	                                        {
		                                        return std::make_unique<FFrequencyPrimitive>(InTasks, OtherInputs);
	                                        });
	InFixture.Tasks.Wait(InFixture.Session.GetScene().Flush());
	for (unsigned Case = 0; Case < 3; ++Case)
	{
		if (Case == 1)
		{
			OtherInputs->FirstId = 7;
		}
		else if (Case == 2)
		{
			InFixture.Inputs->FirstId.reset();
			OtherInputs->FirstId.reset();
		}
		InFixture.DrawKeys.clear();
		InFixture.Build(2);
		HYP_CHECK(InFixture.DrawKeys.size() == 2);
		HYP_CHECK(InFixture.DrawKeys[0] != InFixture.DrawKeys[1]);
		const auto Previous = InFixture.DrawKeys;
		InFixture.DrawKeys.clear();
		InFixture.Build(2);
		HYP_CHECK(InFixture.DrawKeys.size() == 2);
		HYP_CHECK(InFixture.DrawKeys[0] != Previous[0] && InFixture.DrawKeys[1] != Previous[1]);
	}
	InFixture.Tasks.Wait(Other.Remove());
}

void CheckFrequencyChanges(FFrequencyFixture& InFixture, IRHISwapchain& InSwapchain)
{
	const auto Original = InFixture.Build();
	const auto Warm = InFixture.Session.GetResources().Statistics();
	const auto Providers = InFixture.Session.GetProviders().Statistics();
	HYP_CHECK(Providers.Evaluations[static_cast<std::size_t>(EMaterialScope::Object)] == DrawCount);
	InFixture.Time = .01f;
	InFixture.Build();
	auto Current = InFixture.Session.GetResources().Statistics();
	HYP_CHECK(Current.Constants.Packs == Warm.Constants.Packs + 1);
	HYP_CHECK(Current.Constants.FullLookups == Warm.Constants.FullLookups + 1);
	const auto NextProviders = InFixture.Session.GetProviders().Statistics();
	HYP_CHECK(NextProviders.Evaluations[static_cast<std::size_t>(EMaterialScope::Object)] == DrawCount);
	HYP_CHECK(NextProviders.Evaluations[static_cast<std::size_t>(EMaterialScope::View)] == DrawCount + 1);
	InFixture.View.Parameters = {{"Test.View", FMaterialValue::Float(.02f)}};
	InFixture.View.ViewProjection.Values[0] = 2;
	InFixture.Build();
	HYP_CHECK(InFixture.Session.GetResources().Statistics().Constants.Packs == Current.Constants.Packs + 1 + DrawCount);
	Current = InFixture.Session.GetResources().Statistics();
	InFixture.Inputs->Object = .02f;
	InFixture.Build();
	HYP_CHECK(InFixture.Session.GetResources().Statistics().Constants.Packs == Current.Constants.Packs + 2 * DrawCount);
	Current = InFixture.Session.GetResources().Statistics();
	InFixture.Pass = .03f;
	InFixture.Build();
	HYP_CHECK(InFixture.Session.GetResources().Statistics().Constants.Packs == Current.Constants.Packs + 1);
	Current = InFixture.Session.GetResources().Statistics();
	InFixture.Inputs->Draw = .04f;
	const auto Changed = InFixture.Build();
	const auto Final = InFixture.Session.GetResources().Statistics();
	HYP_CHECK(Final.Constants.Packs == Current.Constants.Packs + DrawCount);
	HYP_CHECK(Final.Materials.SetReuses == Warm.Materials.SetReuses);
	HYP_CHECK(Final.Materials.PipelineReuses == Warm.Materials.PipelineReuses);
	const float ObjectTerm = float(DrawCount - 1) * .001f;
	HYP_CHECK(std::abs(InFixture.Render(InSwapchain, Original) - (.06f + 3 * ObjectTerm)) < .01f);
	HYP_CHECK(std::abs(InFixture.Render(InSwapchain, Changed) - (.21f + 3 * ObjectTerm)) < .01f);
	// Equal session writes preserve the immutable publication token even across later frames.
	const auto Before = InFixture.Session.FreezeFrame(InFixture.Time);
	InFixture.Session.SetGlobalParameters({{"Test.Global", FMaterialValue::Float(.01f)}});
	const auto After = InFixture.Session.FreezeFrame(InFixture.Time);
	const auto Global = static_cast<std::size_t>(EMaterialScope::Global);
	HYP_CHECK(Before->Inputs.Scopes[Global].Lifetime == After->Inputs.Scopes[Global].Lifetime);
	HYP_CHECK(Before->Inputs.Values[Global].GetIdentity() == After->Inputs.Values[Global].GetIdentity());
}
} // namespace

void RunMaterialFrequencyTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	FFrequencyFixture Fixture(InDevice);
	CheckFrequencyChanges(Fixture, InSwapchain);
	CheckDrawIdentity(Fixture);
	HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
}
