#include "Renderer/InstanceBatchSupport.h"
#include "Support/GraphTestSupport.h"
#include <chrono>
#include <thread>

namespace Hyperion::InstanceTests
{
FRenderSceneSnapshot Snapshot(FFixture& InFixture, std::size_t InCount)
{
	FRenderSceneSnapshot Result;
	Result.View = InFixture.View;
	Result.Targets = FRenderPassTargets::Frame(ERHIDepthFormat::D32S8);
	for (std::size_t Index = 0; Index < InCount; ++Index)
	{
		FRenderItem Item;
		Item.State.Resource = InFixture.Resource;
		Item.State.Surface = InFixture.Resource->GetMaterial(0);
		Item.Primitive = {99, static_cast<std::uint32_t>(Index), 1};
		Item.Group = Index;
		Item.LocalItemId = 0;
		Item.Lifetime = std::make_shared<int>(0);
		Item.Context.Scopes[static_cast<std::size_t>(EMaterialScope::Object)] = {{99, 1, {Index}}, Item.Lifetime};
		Item.Context.ObjectParameters = {
		    {"Placement",
		     FMaterialValue::Float(FVec4{-.75f + float(Index % 4) * .5f, -.45f + float(Index / 4) * .9f, 0, 1})}};
		const auto Compiled = Item.State.Surface->GetCompiled();
		Item.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(ResolveMaterialBindingContext(
		    Item.State.Surface->GetSnapshot(), *Compiled, Compiled->GetPass(), Item.Context));
		Result.Items.PushBack(std::move(Item));
	}
	return Result;
}

std::vector<FPassCommands> Prepare(FFixture& InFixture, FRenderBatchSystem& InBatches, FRenderSceneSnapshot InSnapshot)
{
	FRenderGraph Graph;
	InFixture.Tasks.Wait(
	    InFixture.Tasks.Dispatch({EDomain::Render},
	                             [&]
	                             {
		                             InSnapshot.Batches = InBatches.Build(InSnapshot);
		                             Graph.Add(MakeColorPass(Graph, "Clear planning", EAttachmentLoad::Clear));
		                             auto Preparation = InFixture.Session->GetResources().GetPreparation();
		                             auto Pass = Preparation.DeclarePass(Graph, InSnapshot);
		                             Pass.Prepare = [Preparation, InSnapshot]
		                             {
			                             return Preparation.BuildDraws(InSnapshot);
		                             };
		                             Graph.Add(std::move(Pass));
	                             }));
	std::vector<FPassCommands> Result;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Rhi, 0},
	                                              [&]
	                                              {
		                                              Result = Graph.Compile();
	                                              }));
	return Result;
}

FMaterialValue Payload(float InRed)
{
	FMaterialValue Value;
	Value.Type.Kind = EMaterialValueKind::Structure;
	Value.Type.MemberNames = {"Color", "Mode", "bVisible", "Sign", "Pair", "Matrix"};
	const std::array<float, 6> Matrix{1, 2, 3, 4, 5, 6};
	Value.Elements = {FMaterialValue::Float(FVec3{InRed, .25f, .3f}),
	                  FMaterialValue::Uint(3),
	                  FMaterialValue::Bool(true),
	                  FMaterialValue::Int(-2),
	                  FMaterialValue::Array({FMaterialValue::Float(FVec2{1, 2}), FMaterialValue::Float(FVec2{3, 4})}),
	                  FMaterialValue::Floats(Matrix, 2)};
	for (const auto& Element : Value.Elements)
	{
		Value.Type.Members.push_back(Element.Type);
	}
	Value.Validate();
	return Value;
}

FMaterialDescription Description()
{
	FMaterialDescription Result;
	Result.Name = "Custom typed instances";
	FMaterialPass Pass;
	Pass.Vertex = {"InstanceData.hlsl", "VSMain"};
	Pass.Pixel = {"InstanceData.hlsl", "PSMain"};
	Pass.InstanceArrays = {{"InstanceData", "Records"}};
	Pass.bAllowBatchReordering = true;
	Pass.bAllowDynamicOverrides = true;
	Pass.State.bDepthTest = true;
	Pass.State.bDepthWrite = true;
	Pass.State.Cull = EMaterialCull::None;
	Result.Passes.push_back(Pass);
	auto Add = [&](std::string InName, FMaterialValue InValue, std::string InTarget)
	{
		FMaterialParameterDeclaration Parameter;
		Parameter.Name = std::move(InName);
		Parameter.Type = InValue.Type;
		Parameter.Default = std::move(InValue);
		Parameter.Targets = {std::move(InTarget)};
		Result.Parameters.push_back(std::move(Parameter));
	};
	Add("Placement", FMaterialValue::Float(FVec4{0, 0, 0, 1}), "InstanceData.Placement");
	Add("Payload", Payload(), "InstanceData.Payload");
	Add("SharedTint", FMaterialValue::Float(FVec4{1, 1, 1, 1}), "SharedData.SharedTint");
	return Result;
}

std::shared_ptr<const FMaterialSnapshot> Surface(FMaterialDescription InDescription)
{
	const auto Texture = std::make_shared<const FMaterialTextureSource>(
	    EMaterialTextureEncoding::Linear, std::vector<FMaterialTextureMip>{{1, 1, {255, 255, 255, 255}}});
	const auto TextureValue = FMaterialValue::FromTexture(Texture);
	const std::array<float, 8> Floats{1, 1, 1, 1, .5f, .5f, .5f, 1};
	const auto Bytes = std::as_bytes(std::span(Floats));
	const auto Buffer = std::make_shared<const FMaterialReadBufferSource>(Bytes);
	const std::array Values{
	    std::pair{"Maps", FMaterialValue::Array({TextureValue, TextureValue})},
	    std::pair{"MapSampler", FMaterialValue::FromSampler({})},
	    std::pair{"ReadData", FMaterialValue::FromBuffer({Buffer, EMaterialBufferViewKind::Structured, 0, 16, 16})}};
	for (const auto& [Name, Value] : Values)
	{
		FMaterialParameterDeclaration Parameter;
		Parameter.Name = Name;
		Parameter.Type = Value.Type;
		Parameter.Default = Value;
		Parameter.Targets = {Name};
		InDescription.Parameters.push_back(std::move(Parameter));
	}
	return FMaterialInstance(std::make_shared<const FMaterialDefinition>(std::move(InDescription))).Freeze();
}

FRenderResourceDesc Geometry(std::shared_ptr<const FMaterialSnapshot> InSurface)
{
	FRenderResourceDesc Result;
	FRenderGeometryDesc Mesh;
	const std::array<FVec3, 3> Vertices{{{-.18f, -.23f, .5f}, {.18f, -.23f, .5f}, {0, .23f, .5f}}};
	const auto Bytes = std::as_bytes(std::span(Vertices));
	Mesh.Vertices.assign(Bytes.begin(), Bytes.end());
	Mesh.Indices = {0, 1, 2, 0, 1, 2};
	Mesh.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0}};
	Mesh.VertexStride = sizeof(FVec3);
	Result.Geometries.push_back(std::move(Mesh));
	Result.Materials.push_back({std::move(InSurface)});
	Result.Sections = {{0, 0, 0, 3}, {0, 0, 3, 3}};
	return Result;
}

void WaitFor(const std::function<bool()>& InCondition)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!InCondition())
	{
		HYP_CHECK(std::chrono::steady_clock::now() < Deadline);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

FItems::FItems(FTaskSystem& InTasks, std::shared_ptr<FEmission> InEmission)
    : IRenderPrimitive(InTasks), Emission(std::move(InEmission))
{
}

void FItems::Collect(const FRenderView&, std::vector<FRenderItem>& OutItems) const
{
	for (std::size_t Ordinal = 0; Ordinal < Emission->Count; ++Ordinal)
	{
		const auto Index = Emission->bReverse ? Emission->Count - 1 - Ordinal : Ordinal;
		if (Emission->Hidden == Index)
		{
			continue;
		}
		FRenderItem Item;
		Item.State = GetState();
		if (Emission->bStable)
		{
			Item.LocalItemId = Index;
		}
		const float X = -.75f + float(Index % 4) * .5f;
		const float Y = -.45f + float(Index / 4) * .9f;
		Item.DrawParameters = {{"Placement", FMaterialValue::Float(FVec4{X, Y, 0, 1})},
		                       {"Payload", Payload(Emission->Changed == Index ? .8f : .1f + float(Index) * .05f)}};
		if (Emission->Invalid == Index)
		{
			Item.DrawParameters.push_back({"Unknown", FMaterialValue::Float(1)});
		}
		if (Emission->Dynamic == Index)
		{
			Item.DynamicState = FMaterialDynamicState{3};
		}
		if (Emission->Alternate == Index)
		{
			Item.State.Surface = Emission->Other;
		}
		OutItems.push_back(std::move(Item));
	}
}

FFixture::FFixture()
{
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          FRHIBackendRegistry Registry;
		                          RegisterD3D12RHIBackend(Registry);
		                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
		                          Swapchain =
		                              Device->CreateSwapchain({Window.Surface(), {128, 96}, ERHIDepthFormat::D32S8});
	                          }));
	Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler, ERHIDepthFormat::D32S8,
	                                           GetStandardMaterialSemantics());
	Resource = Session->GetResources().Request(std::make_shared<int>(1), 1, "instance test",
	                                           []
	                                           {
		                                           return Geometry(Surface());
	                                           });
	WaitFor(
	    [&]
	    {
		    return Resource->GetStatus() == ERenderResourceStatus::Ready ||
		           Resource->GetStatus() == ERenderResourceStatus::Failed;
	    });
	if (Resource->GetStatus() != ERenderResourceStatus::Ready)
	{
		throw std::runtime_error(Resource->GetError());
	}
	const auto Selected = Resource->GetMaterial(0);
	WaitFor(
	    [&]
	    {
		    return Selected->GetStatus() == ERenderMaterialStatus::Ready ||
		           Selected->GetStatus() == ERenderMaterialStatus::Failed;
	    });
	if (Selected->GetStatus() != ERenderMaterialStatus::Ready)
	{
		throw std::runtime_error(Selected->GetError());
	}
	const auto Compiled = Selected->GetCompiled();
	if (!Compiled->FindInstancePass())
	{
		throw std::runtime_error(Compiled->InstanceDiagnostics.empty() ? "Instance pass absent"
		                                                               : Compiled->InstanceDiagnostics.front());
	}
	FRenderPrimitiveState State;
	State.Resource = Resource;
	State.Surface = Resource->GetMaterial(0);
	Binding = Session->GetScene().Create(State,
	                                     [Source = Emission](FTaskSystem& InTasks)
	                                     {
		                                     return std::make_unique<FItems>(InTasks, Source);
	                                     });
	Tasks.Wait(Session->GetScene().Flush());
	View.Width = 128;
	View.Height = 96;
	View.CullingMode = ESceneCullingMode::None;
}

FFixture::~FFixture()
{
	Tasks.Wait(Binding.Remove());
	Emission->Other.reset();
	Resource.reset();
	Session->Close();
	Session.reset();
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Swapchain.reset();
		                          Device.reset();
	                          }));
}

std::vector<FPassCommands> FFixture::Build(bool bInBatch, std::span<const FRenderView> InViews)
{
	std::vector<FPassCommands> Result;
	View.bInstanceBatching = bInBatch;
	const auto Views = InViews.empty() ? std::span(&View, 1) : InViews;
	const auto Frame = Session->FreezeFrame();
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          FRenderGraph Graph;
		                          auto Clear = MakeColorPass(Graph, "Clear");
		                          Clear.Name = "Clear instances";
		                          Clear.Color->Actions.Load = EAttachmentLoad::Clear;
		                          Graph.Add(std::move(Clear));
		                          Session->BuildViews(Graph, Views, Session->FrameTargets(), Frame);
		                          Statistics = Session->Statistics();
		                          Result = Graph.Compile();
	                          }));
	return Result;
}

FImage FFixture::Draw(const std::vector<FPassCommands>& InPasses)
{
	FImage Result;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Swapchain->BeginFrame({128, 96});
		                          try
		                          {
			                          std::vector<FRecordedList> Lists;
			                          for (std::uint32_t Index = 0; Index < InPasses.size(); ++Index)
			                          {
				                          Lists.push_back(Swapchain->Record(Index, InPasses[Index]));
			                          }
			                          Result = Swapchain->EndFrame(Lists, false, true);
		                          }
		                          catch (...)
		                          {
			                          Swapchain->CancelFrame();
			                          throw;
		                          }
	                          }));
	return Result;
}

std::shared_ptr<const FRenderMaterial> FFixture::Material(FMaterialDescription InDescription)
{
	const auto Result = Session->GetResources().RequestMaterial(
	    FMaterialInstance(std::make_shared<const FMaterialDefinition>(std::move(InDescription))).Freeze());
	WaitFor(
	    [&]
	    {
		    return Result->GetStatus() == ERenderMaterialStatus::Ready ||
		           Result->GetStatus() == ERenderMaterialStatus::Failed;
	    });
	if (Result->GetStatus() != ERenderMaterialStatus::Ready)
	{
		throw std::runtime_error(Result->GetError());
	}
	return Result;
}

std::size_t DrawCount(const std::vector<FPassCommands>& InPasses)
{
	std::size_t Result{};
	for (const auto& Pass : InPasses)
	{
		Result += Pass.Draws.size();
	}
	return Result;
}

std::size_t InstanceCount(const std::vector<FPassCommands>& InPasses)
{
	std::size_t Result{};
	for (const auto& Pass : InPasses)
	{
		for (const auto& Draw : Pass.Draws)
		{
			Result += Draw.InstanceCount;
		}
	}
	return Result;
}
} // namespace Hyperion::InstanceTests
