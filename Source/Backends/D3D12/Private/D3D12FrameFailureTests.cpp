// Native fault injection stays inside the backend and is compiled only into this test executable.
#include "D3D12Resources.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Renderer/RenderSession.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <set>
#include <source_location>
#include <thread>

namespace Hyperion
{
namespace
{
bool bFailPresent = false;
bool bSkipPresent = false;

void CheckCondition(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Frame failure check failed at line " + std::to_string(InLocation.line()));
	}
}

struct FFrameFixture
{
	FTaskSystem Tasks{1, 2};
	FWindow Window{"Submitted frame failure", {64, 64}, true};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::shared_ptr<FD3D12DeviceState> State;

	FFrameFixture()
	{
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain({Surface, {64, 64}});
			                          const std::array<std::byte, 4> Bytes{};
			                          auto Buffer = Device->CreateBuffer(Bytes);
			                          State = std::dynamic_pointer_cast<FD3D12Buffer>(Buffer.Payload)->State;
		                          }));
	}

	~FFrameFixture()
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device->WaitIdle();
			                          Swapchain.reset();
			                          Device.reset();
			                          State.reset();
		                          }));
	}
};

FRenderGraph ClearGraph()
{
	FRenderGraph Graph;
	FColorPass Clear;
	Clear.Commands.Name = "Clear";
	Clear.Load = EColorLoad::Clear;
	Clear.Commands.ClearColor = {.25f, .5f, .75f, 1};
	Graph.Add(Clear);
	return Graph;
}

void CheckSubmittedFailure(FFrameFixture& InFixture, const FRenderGraph& InGraph)
{
	auto& Tasks = InFixture.Tasks;
	auto& State = *InFixture.State;
	ComPtr<ID3D12Fence> Gate;
	Check(State.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Gate)), "Create test gate");
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Check(State.Queue->Wait(Gate.Get(), 1), "Block test queue");
	                          }));
	// A healthy queue is deliberately held back; no driver or device-loss fault is induced.
	std::jthread ReleaseGate(
	    [Gate]
	    {
		    std::this_thread::sleep_for(std::chrono::milliseconds(500));
		    Gate->Signal(1);
	    });
	bFailPresent = true;
	bool bFailed = false;
	bool bDrained = false;
	try
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          ExecuteGraph(InGraph, Tasks, *InFixture.Swapchain, {64, 64}, false, false);
		                          }));
	}
	catch (const std::exception& Error)
	{
		bFailed = std::string(Error.what()).find("Present swapchain (HRESULT 0x80004005)") != std::string::npos;
		bDrained = Gate->GetCompletedValue() >= 1 && State.Fence->GetCompletedValue() >= State.NextFence - 1;
	}
	ReleaseGate.join(); // Always unblock the queue before assertions or fixture destruction.
	CheckCondition(bFailed);
	CheckCondition(bDrained);
}

void CheckRecovery(FFrameFixture& InFixture, const FRenderGraph& InGraph)
{
	auto& Tasks = InFixture.Tasks;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          InFixture.Swapchain->CancelFrame();
		                          InFixture.Swapchain->CancelFrame();
	                          }));
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          const auto Image =
		                              ExecuteGraph(InGraph, Tasks, *InFixture.Swapchain, {64, 64}, false, true);
		                          CheckCondition(Image.Width == 64 && std::abs(Image.Rgba[0] - .25f) < .01f);
	                          }));
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          InFixture.Device->WaitIdle();
		                          CheckCondition(InFixture.Device->Statistics().ValidationErrors == 0);
	                          }));
}

void CheckDirectRhiCollection(FFrameFixture& InFixture, const FRenderGraph& InGraph)
{
	auto& Tasks = InFixture.Tasks;
	for (unsigned Index = 0; Index < FrameCount * 4; ++Index)
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          ExecuteGraph(InGraph, Tasks, *InFixture.Swapchain, {64, 64}, false, false);
		                          }));
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          // Waiting proves completion but does not explicitly collect or require global
			                          // idle.
			                          InFixture.State->Wait(InFixture.State->NextFence - 1);
			                          CheckCondition(InFixture.State->Submissions.size() == 1);
		                          }));
	}
}

void CheckRecordingStorage(FFrameFixture& InFixture, const FRenderGraph& InGraph)
{
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Rhi, 0},
	    [&]
	    {
		    const auto Commands = InGraph.Compile();
		    std::shared_ptr<const FD3D12RecordedList> Retained;
		    std::uint64_t FirstFrame{};
		    const auto Before = InFixture.Device->Statistics();
		    for (unsigned Frame = 0; Frame < 12; ++Frame)
		    {
			    InFixture.Swapchain->BeginFrame({64, 64});
			    std::vector<FRecordedList> Lists;
			    for (std::uint32_t Index = 0; Index < Commands.size(); ++Index)
			    {
				    auto Owned = std::make_shared<const FPassCommands>(Commands[Index]);
				    Lists.push_back(InFixture.Swapchain->RecordOwned(Index, Owned));
				    const auto Native = std::dynamic_pointer_cast<const FD3D12RecordedList>(Lists.back().Payload);
				    CheckCondition(Native->Commands == Owned);
			    }
			    const auto Current = std::dynamic_pointer_cast<const FD3D12RecordedList>(Lists.front().Payload);
			    if (Frame == 0)
			    {
				    Retained = Current;
				    FirstFrame = Retained->Frame;
			    }
			    else
			    {
				    CheckCondition(Current->List.Get() != Retained->List.Get());
				    CheckCondition(Retained->Frame == FirstFrame && Retained->Name == Commands.front().Name);
				    CheckCondition(Retained->Commands->Name == Commands.front().Name);
			    }
			    if (Frame == 6)
			    {
				    InFixture.Swapchain->CancelFrame();
			    }
			    else
			    {
				    InFixture.Swapchain->EndFrame(Lists, false);
			    }
			    InFixture.Swapchain->WaitIdle();
		    }
		    const auto After = InFixture.Device->Statistics();
		    CheckCondition(After.CommandListResets > Before.CommandListResets + 12);
		    CheckCondition(After.CommandListsCreated - Before.CommandListsCreated <= FrameCount * Commands.size() + 1);
		    CheckCondition(After.ValidationErrors == 0);
	    }));
}

#if HYP_ENABLE_PROFILING
void CheckProfileQueryLifetime(FFrameFixture& InFixture, const FRenderGraph& InGraph)
{
	if (!GetProfilingConnection())
	{
		return;
	}
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Rhi, 0},
	    [&]
	    {
		    auto& Swapchain = *InFixture.Swapchain;
		    const auto Commands = InGraph.Compile();
		    Swapchain.WaitIdle();
		    Swapchain.BeginFrame({64, 64});
		    auto Cancelled = Swapchain.Record(0, Commands[0]);
		    auto Native = std::dynamic_pointer_cast<FD3D12RecordedList>(Cancelled.Payload);
		    CheckCondition(Native->ProfileQueries != nullptr);
		    std::set<const FD3D12ProfileQueries*> Pools{Native->ProfileQueries.get()};
		    Swapchain.CancelFrame();
		    // A retained cancelled list occupies its fixed slot; repeated frames skip it, without waiting or growing.
		    std::size_t Skipped{};
		    for (unsigned Frame = 0; Frame < FrameCount * 3; ++Frame)
		    {
			    Swapchain.BeginFrame({64, 64});
			    std::vector<FRecordedList> Lists;
			    for (unsigned Index = 0; Index < Commands.size(); ++Index)
			    {
				    Lists.push_back(Swapchain.Record(Index, Commands[Index]));
				    auto Recorded = std::dynamic_pointer_cast<FD3D12RecordedList>(Lists.back().Payload);
				    if (Recorded->ProfileQueries)
				    {
					    Pools.insert(Recorded->ProfileQueries.get());
				    }
				    else
				    {
					    ++Skipped;
				    }
			    }
			    Swapchain.EndFrame(Lists, false, false);
			    Swapchain.WaitIdle();
			    for (const auto& List : Lists)
			    {
				    CheckCondition(!std::dynamic_pointer_cast<FD3D12RecordedList>(List.Payload)->ProfileQueries);
			    }
		    }
		    CheckCondition(Skipped > 0 && Pools.size() <= FrameCount);
		    // No completion event was emitted for the abandoned recording.
		    CheckCondition(Native->ProfileQueries != nullptr);
		    Native.reset();
		    Cancelled = {};
		    Swapchain.BeginFrame({64, 64});
		    const auto Recovered = Swapchain.Record(0, Commands[0]);
		    CheckCondition(std::dynamic_pointer_cast<FD3D12RecordedList>(Recovered.Payload)->ProfileQueries != nullptr);
		    Swapchain.CancelFrame();
		    CheckCondition(InFixture.Device->Statistics().ValidationErrors == 0);
	    }));
}
#endif

class FCountedPrimitive final : public IRenderPrimitive
{
public:
	FCountedPrimitive(FTaskSystem& InTasks, std::atomic_int& InDestroyed)
	    : IRenderPrimitive(InTasks), Destroyed(InDestroyed)
	{
	}

	~FCountedPrimitive() override
	{
		++Destroyed;
	}

	void Collect(const FRenderView&, std::vector<FRenderItem>& OutItems) const override
	{
		OutItems.push_back({GetState(), {}});
	}

private:
	std::atomic_int& Destroyed;
};

FRenderResourceDesc TestTriangle(FShaderCompiler& InCompiler)
{
	FRenderResourceDesc Desc;
	FRenderMaterialDesc Material;
	FMaterialDescription Description;
	Description.Name = "Fence retirement material";
	FMaterialPass Pass;
	Pass.Vertex = {"Gui.hlsl", "VSMain"};
	Pass.Pixel = {"Gui.hlsl", "PSMain"};
	Pass.InstanceArrays = {{"DrawConstants", "DrawInstances"}};
	Description.Passes.push_back(Pass);
	auto Transform =
	    DeclareMaterialSemantic("Transform", "Engine.Object.WorldViewProjection", *GetStandardMaterialSemantics());
	Transform.Targets = {"DrawConstants.TransformMatrix"};
	Description.Parameters.push_back(std::move(Transform));
	FMaterialParameterDeclaration Texture;
	Texture.Name = "FontTexture";
	Texture.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
	Texture.Default = FMaterialValue::FromTexture(std::make_shared<const FMaterialTextureSource>(
	    EMaterialTextureEncoding::Linear, std::vector<FMaterialTextureMip>{{1, 1, {255, 255, 255, 255}}}));
	Description.Parameters.push_back(Texture);
	FMaterialParameterDeclaration Sampler;
	Sampler.Name = "FontSampler";
	Sampler.Type = FMaterialParameterType::Resource(EMaterialValueKind::Sampler);
	Sampler.Default = FMaterialValue::FromSampler({});
	Description.Parameters.push_back(Sampler);
	const auto Definition = std::make_shared<const FMaterialDefinition>(Description);
	Material.Compiled = std::make_shared<const FCompiledMaterialDefinition>(
	    CompileMaterialDefinition(InCompiler, Definition, EShaderFormat::Dxil));
	Material.Surface = FMaterialInstance(Material.Compiled->Interface).Freeze();
	const std::array<FVertex, 3> Vertices{
	    {{{0, .5f, 0}, {1, 0, 0, 1}, {}}, {{-.5f, -.5f, 0}, {0, 1, 0, 1}, {}}, {{.5f, -.5f, 0}, {0, 0, 1, 1}, {}}}};
	FRenderGeometryDesc Geometry;
	const auto Bytes = std::as_bytes(std::span(Vertices));
	Geometry.Vertices.assign(Bytes.begin(), Bytes.end());
	Geometry.Indices = {0, 1, 2};
	Geometry.VertexStride = sizeof(FVertex);
	Geometry.Attributes = {{"POSITION", 0, EVertexFormat::Float2, offsetof(FVertex, Position)},
	                       {"COLOR", 0, EVertexFormat::Float4, offsetof(FVertex, Color)},
	                       {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FVertex, Uv)}};
	Desc.Geometries.push_back(std::move(Geometry));
	Desc.Materials.push_back(std::move(Material));
	Desc.Sections.push_back({0, 0, 0, 3});
	return Desc;
}

std::weak_ptr<IRHISampler> GetSampler(const FResourceBindingSet& InBindings)
{
	const auto Native = std::dynamic_pointer_cast<FD3D12BindingSet>(InBindings.Payload);
	for (const auto& Entry : Native->Description.Entries)
	{
		for (const auto& Value : Entry.Values)
		{
			if (const auto* Source = std::get_if<FSampler>(&Value))
			{
				return Source->Payload;
			}
		}
	}
	throw std::runtime_error("Missing sampler in retirement fixture");
}

void CheckPrimitiveFenceRetirement(FFrameFixture& InFixture)
{
	auto& Tasks = InFixture.Tasks;
	FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
	                         std::filesystem::absolute("frame-failure-shader-cache"));
	FRenderSession Session(Tasks, *InFixture.Device, Compiler);
	auto Resource = Session.GetResources().Request(std::make_shared<const int>(1), 1, "fence-test",
	                                               [&Compiler]
	                                               {
		                                               return TestTriangle(Compiler);
	                                               });
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while ((!Resource->GetMaterial(0) || Resource->GetMaterial(0)->GetStatus() != ERenderMaterialStatus::Ready) &&
	       std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (!Resource->GetError().empty())
	{
		throw std::runtime_error(Resource->GetError());
	}
	CheckCondition(Resource->GetStatus() == ERenderResourceStatus::Ready);
	std::atomic_int Destroyed{};
	FRenderPrimitiveState Initial;
	Initial.Resource = Resource;
	Initial.bClipSpace = true;
	auto Binding = Session.GetScene().Create(std::move(Initial),
	                                         [&Destroyed](FTaskSystem& InTasks)
	                                         {
		                                         return std::make_unique<FCountedPrimitive>(InTasks, Destroyed);
	                                         });
	Tasks.Wait(Session.GetScene().Flush());
	FRenderGraph FailedGraph;
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          FailedGraph = ClearGraph();
		                          Session.Build(FailedGraph, {Identity(), {}, 64, 64});
	                          }));
	CheckSubmittedFailure(InFixture, FailedGraph); // Descriptor tables and CBV pages survive failed Present.
	FailedGraph = {};
	ComPtr<ID3D12Fence> Gate;
	Check(InFixture.State->Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Gate)),
	      "Create retirement gate");
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Check(InFixture.State->Queue->Wait(Gate.Get(), 1), "Block retirement queue");
	                          }));

	// EndFrame publishes its real GPU fence. The test bypasses the OS Present wait only.
	// A scope guard always releases the queue before any resource-owning fixture unwinds.
	struct FGateGuard
	{
		ID3D12Fence* Gate;

		~FGateGuard()
		{
			Gate->Signal(1);
			bSkipPresent = false;
		}
	} Guard{Gate.Get()};

	bSkipPresent = true;
	std::weak_ptr<IRHIBuffer> Vertices;
	std::weak_ptr<IRHIBuffer> Constants;
	std::weak_ptr<IRHIResourceBindingSet> Bindings;
	std::weak_ptr<IRHISampler> Sampler;
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          auto Graph = ClearGraph();
		                          CheckCondition(Session.Build(Graph, {Identity(), {}, 64, 64}) == 1);
		                          const auto Draw = Graph.Compile()[1].Draws[0];
		                          Vertices = Draw.Vertices.Payload;
		                          Constants = Draw.ConstantBindings[0].Slice.Buffer.Payload;
		                          Bindings = Draw.Bindings.Payload;
		                          Sampler = GetSampler(Draw.Bindings);
		                          ExecuteGraph(Graph, Tasks, *InFixture.Swapchain, {64, 64}, false, false);
	                          }));
	Tasks.Wait(Binding.Remove());
	Resource.reset();
	CheckCondition(Destroyed == 1 && !Vertices.expired());
	CheckCondition(!Constants.expired() && !Bindings.expired() && !Sampler.expired());
	CheckCondition(Session.GetResources().Statistics().Retired == 0);
	CheckCondition(Gate->GetCompletedValue() == 0);
	Gate->Signal(1);
	while ((Session.GetResources().Statistics().LiveResources ||
	        Session.GetResources().Statistics().Materials.LiveObjects) &&
	       std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	CheckCondition(Vertices.expired() && Session.GetResources().Statistics().Retired == 1);
	CheckCondition(Bindings.expired() && Sampler.expired());
	Session.Close();
	CheckCondition(Constants.expired());
}

void CheckTimingCaptureBoundary(FFrameFixture& InFixture, const FRenderGraph& InGraph)
{
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Rhi, 0},
	    [&]
	    {
		    auto& Device = *InFixture.Device;
		    auto& Swapchain = *InFixture.Swapchain;
		    auto& State = *InFixture.State;
		    Device.WaitIdle();
		    Swapchain.SetGpuTimingEnabled(true);
		    const auto Commands = InGraph.Compile();
		    const auto Submit = [&]
		    {
			    Swapchain.BeginFrame({64, 64});
			    const std::array Lists{Swapchain.Record(0, Commands[0]), Swapchain.Record(1, Commands[1])};
			    Swapchain.EndFrame(Lists, false, false);
		    };
		    for (const bool bPreviousCapture : {false, true})
		    {
			    ComPtr<ID3D12Fence> Gate;
			    Check(State.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Gate)), "Create timing gate");
			    struct FReleaseGate
			    {
				    ComPtr<ID3D12Fence> Gate;
				    ~FReleaseGate()
				    {
					    Gate->Signal(1);
					    bSkipPresent = false;
				    }
			    } Release{Gate};
			    bSkipPresent = true;
			    Check(State.Queue->Wait(Gate.Get(), 1), "Block timing submission");
			    if (bPreviousCapture)
			    {
				    Device.BeginGpuTimingCapture(2);
			    }
			    Submit();
			    CheckCondition(State.Fence->GetCompletedValue() < State.Submissions.back().FenceValue);
			    if (bPreviousCapture)
			    {
				    CheckCondition(Device.EndGpuTimingCapture().Frames.empty());
			    }
			    Device.BeginGpuTimingCapture(2);
			    Check(Gate->Signal(1), "Release timing submission");
			    Device.WaitIdle();
			    const auto Empty = Device.EndGpuTimingCapture();
			    CheckCondition(Empty.Frames.empty() && Empty.DroppedFrames == 0);
			    CheckCondition(State.LastGpuTiming.Passes.size() == 2);
		    }
		    Device.BeginGpuTimingCapture(2);
		    Submit();
		    Device.WaitIdle();
		    const auto Current = Device.EndGpuTimingCapture();
		    CheckCondition(Current.Frames.size() == 1 && Current.Frames[0].Passes.size() == 2);
		    Swapchain.SetGpuTimingEnabled(false);
	    }));
}

std::shared_ptr<const FPassCommands> OwnedConstantCommands(IRHIDevice& InDevice, bool bInSharedDraws)
{
	FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
	                         std::filesystem::absolute("frame-failure-shader-cache"));
	FPipelineDesc Pipeline;
	Pipeline.Vertex = Compiler.Compile("Triangle.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
	Pipeline.Pixel = Compiler.Compile("Triangle.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
	Pipeline.Attributes = {{"POSITION", 0, EVertexFormat::Float3, offsetof(FVertex, Position)},
	                       {"COLOR", 0, EVertexFormat::Float4, offsetof(FVertex, Color)},
	                       {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FVertex, Uv)}};
	Pipeline.VertexStride = sizeof(FVertex);
	Pipeline.Layout =
	    InDevice.CreateBindingLayout({{{ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Vertex, 0, 0, 1, 64},
	                                   {ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel, 0}}});
	const std::array<FVertex, 3> Vertices{
	    {{{0, .5f, 0}, {1, 0, 0, 1}, {}}, {{-.5f, -.5f, 0}, {0, 1, 0, 1}, {}}, {{.5f, -.5f, 0}, {0, 0, 1, 1}, {}}}};
	const std::array<std::uint32_t, 3> Indices{0, 1, 2};
	FDrawPacket Draw;
	Draw.Pipeline = InDevice.CreatePipeline(Pipeline);
	Draw.Vertices = InDevice.CreateBuffer(std::as_bytes(std::span(Vertices)));
	Draw.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	Draw.VertexStride = sizeof(FVertex);
	Draw.IndexCount = 3;
	Draw.Scissor = {0, 0, 64, 64};
	const auto Texture = InDevice.CreateTexture({1, 1, EColorSpace::Linear, {1, 1, 1, 1}});
	Draw.Bindings = InDevice.CreateBindingSet({Pipeline.Layout, {{1, {Texture}}}});
	const auto Page = InDevice.CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
	const auto Transform = Identity();
	Draw.ConstantBindings = {{0, InDevice.PublishConstantSlice(Page, 0, std::as_bytes(std::span(&Transform, 1)))}};
	FPassCommands Commands;
	Commands.Name = "Owned constants";
	Commands.bClear = true;
	Commands.TransitionFrom = EResourceState::Present;
	Commands.TransitionTo = EResourceState::RenderTarget;
	Commands.Draws.push_back(std::move(Draw));
	if (bInSharedDraws)
	{
		Commands.ShareDraws();
	}
	return std::make_shared<const FPassCommands>(std::move(Commands));
}

void CheckOwnedConstantSubmission(FFrameFixture& InFixture, bool bInSharedDraws)
{
	auto& Device = *InFixture.Device;
	auto& Swapchain = *InFixture.Swapchain;
	auto& State = *InFixture.State;
	auto Commands = OwnedConstantCommands(Device, bInSharedDraws);
	const auto& NestedPage = Commands->GetDraws()[0].ConstantBindings[0].Slice.Buffer;
	Device.WaitIdle();
	Swapchain.BeginFrame({64, 64});
	ComPtr<ID3D12Fence> Gate;
	Check(State.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Gate)), "Create constant ownership gate");

	struct FReleaseGate
	{
		ComPtr<ID3D12Fence> Gate;

		~FReleaseGate()
		{
			Gate->Signal(1);
			bSkipPresent = false;
		}
	} Release{Gate};

	bSkipPresent = true;
	Check(State.Queue->Wait(Gate.Get(), 1), "Block constant ownership submission");
	{
		FPassCommands Present;
		Present.TransitionFrom = EResourceState::RenderTarget;
		Present.TransitionTo = EResourceState::Present;
		const std::array Lists{Swapchain.RecordOwned(0, Commands), Swapchain.Record(1, Present)};
		Swapchain.EndFrame(Lists, false, false);
	}
	Commands.reset(); // Only native submission/frame owners now keep the nested page reference valid.
	CheckCondition(State.Fence->GetCompletedValue() < State.Submissions.back().FenceValue);
	CheckCondition(NestedPage.Payload.use_count() == 1);
	bool bRejected = false;
	try
	{
		Device.ResetConstantBuffer(NestedPage);
	}
	catch (const std::logic_error&)
	{
		bRejected = true;
	}
	CheckCondition(bRejected);
	const auto ReusablePage = NestedPage;
	Check(Gate->Signal(1), "Release constant ownership submission");
	Swapchain.WaitIdle();
	CheckCondition(ReusablePage.Payload.use_count() == 1);
	Device.ResetConstantBuffer(ReusablePage);
	CheckCondition(Device.Statistics().ValidationErrors == 0);
}
} // namespace

HRESULT PresentForTesting(IDXGISwapChain3* InSwapchain, UINT InInterval)
{
	if (bFailPresent)
	{
		bFailPresent = false;
		return E_FAIL;
	}
	return bSkipPresent ? S_OK : InSwapchain->Present(InInterval, 0);
}
} // namespace Hyperion

int main(int InArgc, char** InArgv)
{
	try
	{
#if HYP_ENABLE_PROFILING
		if (InArgc == 2 && std::string_view(InArgv[1]) == "--profile-wait")
		{
			Hyperion::SetProfilingMask(Hyperion::ProfileAllMask);
			const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
			while (!Hyperion::GetProfilingConnection() && std::chrono::steady_clock::now() < Deadline)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			Hyperion::CheckCondition(Hyperion::GetProfilingConnection() != 0);
		}
#else
		(void)InArgc;
		(void)InArgv;
#endif
		Hyperion::FFrameFixture Fixture;
		const auto Graph = Hyperion::ClearGraph();
		Hyperion::CheckSubmittedFailure(Fixture, Graph);
		Hyperion::CheckRecovery(Fixture, Graph);
		Hyperion::CheckDirectRhiCollection(Fixture, Graph);
		Hyperion::CheckRecordingStorage(Fixture, Graph);
		Hyperion::CheckTimingCaptureBoundary(Fixture, Graph);
		Fixture.Tasks.Wait(Fixture.Tasks.Dispatch({Hyperion::EDomain::Rhi, 0},
		                                          [&]
		                                          {
			                                          Hyperion::CheckOwnedConstantSubmission(Fixture, true);
			                                          Hyperion::CheckOwnedConstantSubmission(Fixture, false);
		                                          }));
		Hyperion::CheckPrimitiveFenceRetirement(Fixture);
#if HYP_ENABLE_PROFILING
		Hyperion::CheckProfileQueryLifetime(Fixture, Graph);
#endif
		std::cout << "Submitted frame failure drains the GPU and permits recovery\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
