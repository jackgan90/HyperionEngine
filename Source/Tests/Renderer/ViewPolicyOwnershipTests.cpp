#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/TransientGeometry.h"
#include "Support/RecordedDrawSupport.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <thread>

using namespace Hyperion;

namespace
{
void WritePolicyShader()
{
	const std::string_view Source = R"SHADER(struct FPolicyVertex
{
	float4 Position : SV_Position;
	float4 Color : COLOR0;
};

FPolicyVertex PolicyVertex(float3 InPosition : POSITION, float4 InColor : COLOR0)
{
	FPolicyVertex Result;
	Result.Position = float4(InPosition, 1);
	Result.Color = InColor;
	return Result;
}

float4 PolicyPixel(FPolicyVertex InInput) : SV_Target0
{
	return InInput.Color;
}
)SHADER";
	FLocalFileSystem Files;
	Files.WriteAtomic(TestShaderRoot() / "ViewPolicyTest.hlsl", std::as_bytes(std::span(Source)));
}

std::shared_ptr<const FCompiledMaterialDefinition> CompilePolicyMaterial(FShaderCompiler& InCompiler)
{
	FMaterialDescription Description;
	Description.Name = "view policy ownership";
	for (const std::string_view Usage : {"Forward", "ShadowDepth", "CustomShadowTest"})
	{
		FMaterialPass Pass;
		Pass.Usage = Usage;
		Pass.Vertex = {"ViewPolicyTest.hlsl", "PolicyVertex"};
		Pass.Pixel = {"ViewPolicyTest.hlsl", "PolicyPixel"};
		Pass.State.bDepthTest = false;
		Pass.State.bDepthWrite = false;
		Pass.State.Cull = EMaterialCull::None;
		Description.Passes.push_back(std::move(Pass));
	}
	return std::make_shared<const FCompiledMaterialDefinition>(CompileMaterialDefinition(
	    InCompiler, std::make_shared<const FMaterialDefinition>(std::move(Description)), EShaderFormat::Dxil));
}

FRenderResourceDesc PolicyGeometry(std::shared_ptr<const FCompiledMaterialDefinition> InProgram, std::size_t InIndex)
{
	struct FVertex
	{
		FVec3 Position;
		FVec4 Color;
	};

	const std::array Centers{-.625f, 0.f, .625f};
	const std::array Colors{FVec4{1, 0, 0, 1}, FVec4{0, 1, 0, 1}, FVec4{0, 0, 1, 1}};
	const auto Center = Centers[InIndex];
	const auto Color = Colors[InIndex];
	const std::array Vertices{FVertex{{Center - .2f, -.5f, .5f}, Color}, FVertex{{Center + .2f, -.5f, .5f}, Color},
	                          FVertex{{Center + .2f, .5f, .5f}, Color}, FVertex{{Center - .2f, .5f, .5f}, Color}};
	FRenderResourceDesc Result;
	FRenderGeometryDesc Geometry;
	Geometry.Vertices.resize(sizeof(Vertices));
	std::memcpy(Geometry.Vertices.data(), Vertices.data(), sizeof(Vertices));
	Geometry.VertexStride = sizeof(FVertex);
	Geometry.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0},
	                       {"COLOR", 0, EVertexFormat::Float4, offsetof(FVertex, Color)}};
	Geometry.Indices = {0, 1, 2, 0, 2, 3};
	Geometry.Bounds = {{Center - .2f, -.5f, .5f}, {Center + .2f, .5f, .5f}, true};
	Result.Geometries.push_back(std::move(Geometry));
	Result.Materials.push_back({FMaterialInstance(InProgram->Interface).Freeze(), std::move(InProgram)});
	Result.Sections.push_back({0, 0, 0, 6});
	return Result;
}

struct FPolicyFrame
{
	FRenderGraph Graph;
	FRenderViewPreparation Preparation;
	std::vector<std::shared_ptr<const FPassCommands>> Commands;
	FImage Image;
};

class FPolicyFixture
{
public:
	FPolicyFixture(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
	    : Tasks(InTasks), Swapchain(InSwapchain), Compiler(TestShaderRoot(), "view-policy/cache"),
	      Session(InTasks, InDevice, Compiler, ERHIDepthFormat::D32S8, GetStandardMaterialSemantics())
	{
		WritePolicyShader();
		const auto Program = CompilePolicyMaterial(Compiler);
		for (std::size_t Index = 0; Index < States.size(); ++Index)
		{
			States[Index].Resource =
			    Session.GetResources().Request(std::make_shared<const std::size_t>(Index), 1, "view-policy-ownership",
			                                   [Program, Index]
			                                   {
				                                   return PolicyGeometry(Program, Index);
			                                   });
		}
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
		while (!ResourcesReady() && std::chrono::steady_clock::now() < Deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		HYP_CHECK(ResourcesReady());
		for (auto& State : States)
		{
			State.Surface = State.Resource->GetMaterial(0);
		}
		Bindings = Session.GetScene().CreateBatch({States[0], States[1]});
		Tasks.Wait(Session.GetScene().Flush());
		Transient.Lifetime = std::make_shared<const int>(1);
		Transient.SceneItems = {States[2]};
		Transient.ReplacedPrimitives = {Bindings[0].GetHandle()};
	}

	FPolicyFrame Build(const FRenderView& InView, bool bInDeferred, const FTransientGeometry* InTransient = nullptr)
	{
		FPolicyFrame Result;
		const auto Frame = Session.FreezeFrame();
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          const std::array Targets{Session.FrameTargets(FVec4{0, 0, 0, 1})};
			                          Session.BuildViews(Result.Graph, std::span(&InView, 1), Targets, Frame, 1, false,
			                                             bInDeferred, {}, InTransient);
			                          Result.Preparation = Session.GetViewPreparation();
		                          }));
		return Result;
	}

	void Execute(FPolicyFrame& InFrame)
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FObservedSwapchain Observed(Swapchain, InFrame.Commands);
			                          InFrame.Image = ExecuteGraphOnRhi(std::move(InFrame.Graph), Tasks, Observed,
			                                                            {128, 96}, false, true);
		                          }));
	}

	FRenderView View(std::string_view InUsage) const
	{
		FRenderView Result;
		Result.Usage = InUsage;
		Result.Width = 128;
		Result.Height = 96;
		Result.CullingMode = ESceneCullingMode::None;
		Result.bInstanceBatching = false;
		return Result;
	}

	const FTransientGeometry& GetTransient() const
	{
		return Transient;
	}

private:
	bool ResourcesReady() const
	{
		for (const auto& State : States)
		{
			if (!State.Resource->GetError().empty())
			{
				throw std::runtime_error(State.Resource->GetError());
			}
			const auto Material = State.Resource->GetMaterial(0);
			if (!Material)
			{
				return false;
			}
			if (!Material->GetError().empty())
			{
				throw std::runtime_error(Material->GetError());
			}
			if (State.Resource->GetStatus() != ERenderResourceStatus::Ready ||
			    Material->GetStatus() != ERenderMaterialStatus::Ready)
			{
				return false;
			}
		}
		return true;
	}

	FTaskSystem& Tasks;
	IRHISwapchain& Swapchain;
	FShaderCompiler Compiler;
	FRenderSession Session;
	std::array<FRenderPrimitiveState, 3> States;
	std::vector<FRenderBinding> Bindings;
	FTransientGeometry Transient;
};

void CheckPolicyFrame(const FPolicyFrame& InFrame, std::string_view InUsage, ERenderViewStatsCategory InCategory,
                      std::span<const std::size_t> InExpected)
{
	const auto Stats = InFrame.Preparation.Statistics();
	HYP_CHECK(Stats.Views.size() == 1 && Stats.Views[0].Identity == 1 && Stats.Views[0].Usage == InUsage);
	HYP_CHECK(Stats.Views[0].StatsCategory == InCategory);
	HYP_CHECK(Stats.Views[0].Visibility.VisibleItems == InExpected.size());
	HYP_CHECK(Stats.Views[0].Visibility.Draws == InExpected.size());
	HYP_CHECK(InFrame.Commands.size() == 2 && InFrame.Commands[0] && InFrame.Commands[1]);
	HYP_CHECK(InFrame.Commands[0]->Draws.empty() && InFrame.Commands[0]->SharedDraws);
	HYP_CHECK(InFrame.Commands[0]->GetDraws().size() == InExpected.size());
	for (const auto& Draw : InFrame.Commands[0]->GetDraws())
	{
		HYP_CHECK(Draw.IndexCount == 6 && Draw.InstanceCount == 1);
	}
	HYP_CHECK(InFrame.Commands[1]->Name == "Graph exports" && InFrame.Commands[1]->GetDraws().empty());
	HYP_CHECK(InFrame.Image.Width == 128 && InFrame.Image.Height == 96);
	const std::array Columns{24u, 64u, 104u};
	for (std::size_t Index = 0; Index < Columns.size(); ++Index)
	{
		const bool bPresent = std::find(InExpected.begin(), InExpected.end(), Index) != InExpected.end();
		const auto Pixel = (48 * InFrame.Image.Width + Columns[Index]) * 4;
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			const auto Actual = InFrame.Image.Rgba[Pixel + Channel];
			HYP_CHECK(bPresent && Channel == Index ? Actual > .95f : Actual < .01f);
		}
	}
}

void CheckPolicyMatrix(FPolicyFixture& InFixture, std::string_view InUsage, ERenderViewStatsCategory InCategory,
                       bool bInDeferred)
{
	auto View = InFixture.View(InUsage);
	View.Policy.StatsCategory = InCategory;
	const std::array<std::pair<bool, bool>, 4> Effects{{{false, false}, {true, false}, {false, true}, {true, true}}};
	const std::array<std::vector<std::size_t>, 4> Expected{{{0, 1}, {0, 1, 2}, {1}, {1, 2}}};
	for (std::size_t Index = 0; Index < Effects.size(); ++Index)
	{
		View.Policy.bAddTransientSceneItems = Effects[Index].first;
		View.Policy.bApplyTransientReplacements = Effects[Index].second;
		auto Frame = InFixture.Build(View, bInDeferred, &InFixture.GetTransient());
		InFixture.Execute(Frame);
		CheckPolicyFrame(Frame, InUsage, InCategory, Expected[Index]);
	}
}

void CheckOwnedPolicy(FPolicyFixture& InFixture, std::string_view InUsage, bool bInDeferred)
{
	auto View = InFixture.View(InUsage);
	auto Warm = InFixture.Build(View, false);
	InFixture.Execute(Warm);
	auto First = InFixture.Build(View, bInDeferred);
	View.Policy.StatsCategory = ERenderViewStatsCategory::Shadow;
	auto Second = InFixture.Build(View, bInDeferred);
	View.Policy = {false, true, ERenderViewStatsCategory::Uncounted};
	auto Third = InFixture.Build(View, bInDeferred, &InFixture.GetTransient());
	// No graph executes until all three owned snapshots exist; deferred preparation remains unopened.
	bool bUnavailable{};
	try
	{
		const auto Stats = First.Preparation.Statistics();
		HYP_CHECK(Stats.Views[0].StatsCategory == ERenderViewStatsCategory::Main);
	}
	catch (const std::logic_error&)
	{
		bUnavailable = true;
	}
	HYP_CHECK(bUnavailable == bInDeferred);
	const std::array<std::size_t, 2> Persistent{0, 1};
	const std::array<std::size_t, 1> Replaced{1};
	InFixture.Execute(First);
	InFixture.Execute(Second);
	InFixture.Execute(Third);
	CheckPolicyFrame(First, InUsage, ERenderViewStatsCategory::Main, Persistent);
	CheckPolicyFrame(Second, InUsage, ERenderViewStatsCategory::Shadow, Persistent);
	CheckPolicyFrame(Third, InUsage, ERenderViewStatsCategory::Uncounted, Replaced);
	HYP_CHECK(First.Commands[0]->SharedDraws == Second.Commands[0]->SharedDraws);
	HYP_CHECK(First.Image.Rgba == Second.Image.Rgba && First.Image.Rgba != Third.Image.Rgba);
	for (const auto* Frame : {&First, &Second})
	{
		const auto Stats = Frame->Preparation.Statistics().Views[0].Visibility;
		HYP_CHECK(Stats.CollectionReuses == 1 && Stats.PreparationReuses == 1 && Stats.PacketReuses == 1);
	}
	// Re-read the first receipt after subsequent policy changes and all native work.
	CheckPolicyFrame(First, InUsage, ERenderViewStatsCategory::Main, Persistent);
}
} // namespace

void RunViewPolicyOwnershipTests(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	FPolicyFixture Fixture(InTasks, InDevice, InSwapchain);
	const std::array<std::pair<std::string_view, ERenderViewStatsCategory>, 2> Cases{
	    {{"CustomShadowTest", ERenderViewStatsCategory::Shadow}, {"ShadowDepth", ERenderViewStatsCategory::Main}}};
	for (const auto& Entry : Cases)
	{
		for (const bool bDeferred : {false, true})
		{
			CheckPolicyMatrix(Fixture, Entry.first, Entry.second, bDeferred);
			CheckOwnedPolicy(Fixture, Entry.first, bDeferred);
		}
	}
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              InDevice.WaitIdle();
		                              HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
	                              }));
}
