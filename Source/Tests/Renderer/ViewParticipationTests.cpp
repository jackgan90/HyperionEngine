#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/TransientGeometry.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>

using namespace Hyperion;

namespace
{
FRenderResourceDesc ParticipationGeometry(FRenderResourceDesc InGeometry)
{
	const auto& Original = InGeometry.Materials.at(0);
	auto Description = Original.Surface->Definition->GetDescription();
	const auto OriginalPass = Description.Passes.at(0);
	Description.Passes.clear();
	auto Program = std::make_shared<FCompiledMaterialDefinition>();
	for (const std::string_view Usage : {"Forward", "ShadowDepth", "CustomShadowTest"})
	{
		auto Pass = OriginalPass;
		Pass.Usage = Usage;
		Description.Passes.push_back(std::move(Pass));
		auto Compiled = Original.Compiled->Passes.at(0);
		Compiled.Usage = Usage;
		Program->Passes.push_back(std::move(Compiled));
	}
	Program->Interface.Definition = std::make_shared<const FMaterialDefinition>(std::move(Description));
	const auto& PreparedDescription = Program->Interface.Definition->GetDescription();
	Program->Interface.Schema =
	    std::make_shared<const FMaterialParameterSchema>(PreparedDescription.Parameters, PreparedDescription.Version);
	InGeometry.Materials = {{FMaterialInstance(Program->Interface).Freeze(), Program}};
	return InGeometry;
}

struct FParticipationFrame
{
	std::vector<FPassCommands> Commands;
	FRenderViewPreparation Preparation;
	FSceneVisibilityStats Compatibility;
};

class FParticipationFixture
{
public:
	FParticipationFixture(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
	                      const FRenderResourceDesc& InGeometry)
	    : Tasks(InTasks), Session(InTasks, InDevice, InCompiler)
	{
		for (std::size_t Index = 0; Index < States.size(); ++Index)
		{
			auto& State = States[Index];
			State.Resource =
			    Session.GetResources().Request(std::make_shared<const std::size_t>(Index), 1, "view-participation",
			                                   [InGeometry]
			                                   {
				                                   return ParticipationGeometry(InGeometry);
			                                   });
			State.World = Translation({static_cast<float>(Index), 0, 0});
			State.Revision = Index + 1;
		}
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!ResourcesReady() && std::chrono::steady_clock::now() < Deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		HYP_CHECK(ResourcesReady());
		for (auto& State : States)
		{
			State.Surface = State.Resource->GetMaterial(0);
			HYP_CHECK(State.Surface && State.Surface->GetStatus() == ERenderMaterialStatus::Ready);
			for (const std::string_view Usage : {"Forward", "ShadowDepth", "CustomShadowTest"})
			{
				HYP_CHECK(State.Surface->GetSnapshot()->Definition->HasPass(Usage));
				HYP_CHECK(std::count_if(State.Surface->GetCompiled()->Passes.begin(),
				                        State.Surface->GetCompiled()->Passes.end(),
				                        [Usage](const auto& InPass)
				                        {
					                        return InPass.Usage == Usage;
				                        }) == 1);
			}
		}
		Bindings = Session.GetScene().CreateBatch({States[0], States[1]});
		Tasks.Wait(Session.GetScene().Flush());
		for (const auto& Binding : Bindings)
		{
			HYP_CHECK(Binding.GetStatus().State == ERenderPrimitiveStatus::Ready);
		}
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [this]
		                          {
			                          CaptureGeometry();
		                          }));
	}

	FRenderView View(std::string_view InUsage) const
	{
		FRenderView Result;
		Result.Usage = InUsage;
		Result.CullingMode = ESceneCullingMode::None;
		Result.bSkipMissingPass = true;
		Result.bInstanceBatching = false;
		return Result;
	}

	FTransientGeometry Transient(bool bInAdd, bool bInReplace) const
	{
		FTransientGeometry Result;
		Result.Lifetime = Lifetime;
		if (bInAdd)
		{
			Result.SceneItems.push_back(States[2]);
		}
		if (bInReplace)
		{
			Result.ReplacedPrimitives.push_back(Bindings[0].GetHandle());
		}
		return Result;
	}

	FParticipationFrame Build(FRenderView InView, const FTransientGeometry* InTransient = nullptr)
	{
		const auto Frame = Session.FreezeFrame();
		FParticipationFrame Result;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          FRenderGraph Graph;
			                          const std::array Targets{Session.FrameTargets(FVec4{}, InView.DepthConvention)};
			                          Session.BuildViews(Graph, std::span(&InView, 1), Targets, Frame, 1, false, false,
			                                             {}, InTransient);
			                          Result.Preparation = Session.GetViewPreparation();
			                          Result.Compatibility = Session.Statistics();
			                          Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                                                    [&]
			                                                    {
				                                                    Result.Commands = Graph.CompileAndConsume();
			                                                    }));
		                          }));
		return Result;
	}

	void CheckCollection(const FRenderView& InView, const FTransientGeometry& InTransient,
	                     std::span<const std::size_t> InExpected)
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          auto Snapshot = Session.GetScene().Collect(InView);
			                          Snapshot.Targets = Session.FrameTargets();
			                          Snapshot = PrepareSceneSnapshot(std::move(Snapshot));
			                          HYP_CHECK(Snapshot.Items.Size() == 2);
			                          const std::array PersistentLifetimes{Snapshot.Items[0].Lifetime,
			                                                               Snapshot.Items[1].Lifetime};
			                          AppendTransientSceneItems(Snapshot, InTransient);
			                          HYP_CHECK(Snapshot.Items.Size() == InExpected.size());
			                          HYP_CHECK(Snapshot.Statistics.VisibleItems == InExpected.size());
			                          for (std::size_t Index = 0; Index < InExpected.size(); ++Index)
			                          {
				                          const auto Expected = InExpected[Index];
				                          const auto& Item = Snapshot.Items[Index];
				                          HYP_CHECK(Item.State.Resource == States[Expected].Resource);
				                          HYP_CHECK(Item.State.Surface == States[Expected].Surface);
				                          HYP_CHECK(Item.State.Revision == States[Expected].Revision);
				                          HYP_CHECK(Item.State.World.Values == States[Expected].World.Values);
				                          if (Expected < 2)
				                          {
					                          HYP_CHECK(Item.Primitive == Bindings[Expected].GetHandle());
					                          HYP_CHECK(Item.Lifetime == PersistentLifetimes[Expected]);
				                          }
				                          else
				                          {
					                          const FRenderPrimitiveHandle ExpectedHandle{
					                              0x7472616e7369656e, static_cast<std::uint32_t>(Index), 1};
					                          HYP_CHECK(Item.Primitive == ExpectedHandle && Item.Lifetime == Lifetime);
				                          }
			                          }
		                          }));
	}

	void CheckFrame(const FParticipationFrame& InFrame, std::string_view InUsage,
	                std::span<const std::size_t> InExpected) const
	{
		const auto Statistics = InFrame.Preparation.Statistics();
		HYP_CHECK(Statistics.Views.size() == 1);
		HYP_CHECK(Statistics.Views[0].Identity == 1 && Statistics.Views[0].Usage == InUsage);
		HYP_CHECK(Statistics.Views[0].Visibility.VisibleItems == InExpected.size());
		HYP_CHECK(Statistics.Views[0].Visibility.Draws == InExpected.size());
		HYP_CHECK(InFrame.Compatibility.VisibleItems == InExpected.size());
		HYP_CHECK(InFrame.Compatibility.Draws == InExpected.size());
		HYP_CHECK(InFrame.Commands.size() == 2);
		const auto& Commands = InFrame.Commands.front();
		HYP_CHECK(Commands.Draws.empty() && Commands.SharedDraws);
		std::vector<const void*> Actual;
		for (const auto& Draw : Commands.GetDraws())
		{
			HYP_CHECK(Draw.IndexCount == 3 && Draw.InstanceCount == 1);
			Actual.push_back(Draw.Vertices.Payload.get());
		}
		std::vector<const void*> Expected;
		for (const auto Index : InExpected)
		{
			Expected.push_back(GeometryPayloads[Index]);
		}
		std::sort(Actual.begin(), Actual.end());
		std::sort(Expected.begin(), Expected.end());
		HYP_CHECK(Actual == Expected);
		const auto& Exports = InFrame.Commands.back();
		HYP_CHECK(Exports.Name == "Graph exports" && Exports.GetDraws().empty());
		HYP_CHECK(Exports.GetColors().empty() && !Exports.DepthStencil);
		HYP_CHECK(Exports.Transitions.size() == 1);
		const auto& Transition = Exports.Transitions.front();
		HYP_CHECK(Transition.Target == FRenderTarget::Backbuffer());
		HYP_CHECK(Transition.Before == EResourceState::RenderTarget && Transition.After == EResourceState::Present);
	}

	FTaskSystem& Tasks;
	FRenderSession Session;
	std::array<FRenderPrimitiveState, 3> States;
	std::vector<FRenderBinding> Bindings;
	std::array<const void*, 3> GeometryPayloads{};
	std::shared_ptr<const void> Lifetime = std::make_shared<const int>(7);

private:
	bool ResourcesReady() const
	{
		return std::all_of(States.begin(), States.end(),
		                   [](const auto& InState)
		                   {
			                   const auto ResourceError = InState.Resource->GetError();
			                   if (!ResourceError.empty())
			                   {
				                   throw std::runtime_error(ResourceError);
			                   }
			                   const auto Material = InState.Resource->GetMaterial(0);
			                   if (!Material)
			                   {
				                   return false;
			                   }
			                   const auto MaterialError = Material->GetError();
			                   if (!MaterialError.empty())
			                   {
				                   throw std::runtime_error(MaterialError);
			                   }
			                   return InState.Resource->GetStatus() == ERenderResourceStatus::Ready &&
			                          Material->GetStatus() == ERenderMaterialStatus::Ready;
		                   });
	}

	void CaptureGeometry()
	{
		for (std::size_t Index = 0; Index < States.size(); ++Index)
		{
			FRenderSceneSnapshot Snapshot;
			Snapshot.View = View("Forward");
			Snapshot.Targets = Session.FrameTargets();
			FRenderItem Item;
			Item.State = States[Index];
			Item.Lifetime = Lifetime;
			Snapshot.Items.PushBack(std::move(Item));
			Snapshot = PrepareSceneSnapshot(std::move(Snapshot));
			Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                          [&]
			                          {
				                          const auto Batches = Session.GetResources().BuildDraws(Snapshot);
				                          HYP_CHECK(Batches.size() == 1 && Batches[0].Commands.GetDraws().size() == 1);
				                          GeometryPayloads[Index] =
				                              Batches[0].Commands.GetDraws()[0].Vertices.Payload.get();
			                          }));
		}
		HYP_CHECK(GeometryPayloads[0] != GeometryPayloads[1] && GeometryPayloads[1] != GeometryPayloads[2]);
	}
};

void CheckLegacyParticipation(FParticipationFixture& InFixture, std::string_view InUsage, bool bInShadow)
{
	const auto View = InFixture.View(InUsage);
	const std::array<std::size_t, 2> Persistent{0, 1};
	const std::array<std::size_t, 3> Added{0, 1, 2};
	const std::array<std::size_t, 1> Removed{1};
	const std::array<std::size_t, 2> Replaced{1, 2};
	const auto Warm = InFixture.Build(View);
	const auto Original = InFixture.Build(View);
	const auto Stable = Original.Preparation.Statistics().Views[0].Visibility;
	HYP_CHECK(Stable.CollectionReuses == 1 && Stable.PreparationReuses == 1 && Stable.PacketReuses == 1);
	HYP_CHECK(Original.Commands.size() == Warm.Commands.size());
	for (std::size_t Index = 0; Index < Original.Commands.size(); ++Index)
	{
		HYP_CHECK(Original.Commands[Index].SharedDraws == Warm.Commands[Index].SharedDraws);
	}
	InFixture.CheckFrame(Original, InUsage, Persistent);
	for (const auto& Entry :
	     std::array<std::pair<bool, bool>, 4>{{{false, false}, {true, false}, {false, true}, {true, true}}})
	{
		const auto Transient = InFixture.Transient(Entry.first, Entry.second);
		const std::span<const std::size_t> Expected =
		    Entry.second ? (Entry.first && !bInShadow ? std::span<const std::size_t>(Replaced) : Removed)
		                 : (Entry.first && !bInShadow ? std::span<const std::size_t>(Added) : Persistent);
		InFixture.CheckCollection(View, Transient, Expected);
		const auto Frame = InFixture.Build(View, &Transient);
		InFixture.CheckFrame(Frame, InUsage, Expected);
		InFixture.CheckFrame(Original, InUsage, Persistent);
	}
	for (const bool bWrongScene : {false, true})
	{
		auto Transient = InFixture.Transient(false, true);
		if (bWrongScene)
		{
			++Transient.ReplacedPrimitives[0].Scene;
		}
		else
		{
			++Transient.ReplacedPrimitives[0].Generation;
		}
		InFixture.CheckCollection(View, Transient, Persistent);
		InFixture.CheckFrame(InFixture.Build(View, &Transient), InUsage, Persistent);
	}
	InFixture.CheckFrame(InFixture.Build(View), InUsage, Persistent);
}
} // namespace

void RunViewParticipationTests(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                               const FRenderResourceDesc& InGeometry)
{
	FParticipationFixture Fixture(InTasks, InDevice, InCompiler, InGeometry);
	CheckLegacyParticipation(Fixture, "Forward", false);
	CheckLegacyParticipation(Fixture, "CustomShadowTest", false);
	CheckLegacyParticipation(Fixture, "ShadowDepth", true);
}
