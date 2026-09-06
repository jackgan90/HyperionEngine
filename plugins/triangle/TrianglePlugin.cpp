#include "hyperion/RenderGraph.h"
#include <array>
#include <cstddef>

namespace Hyperion
{
namespace
{
class FTrianglePlugin final : public IRenderPlugin
{
public:
	FTrianglePlugin(FRhiDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks)
	    : Device(InDevice), Compiler(InCompiler), Tasks(InTasks)
	{
	}

	void Start() override
	{
		FPipelineDesc Desc;
		Tasks.Wait(Tasks.Dispatch(
		    {EDomain::Worker},
		    [&]
		    {
			    Desc.Vertex = Compiler.Compile("Triangle.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
			    Desc.Pixel = Compiler.Compile("Triangle.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
		    }));
		Desc.Attributes = {{"POSITION", 0, EVertexFormat::Float3, offsetof(FVertex, Position)},
		                   {"COLOR", 0, EVertexFormat::Float4, offsetof(FVertex, Color)},
		                   {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FVertex, Uv)}};
		Draw.Pipeline = Device.CreatePipeline(Desc);
		const std::array<FVertex, 3> Vertices{{{{0, .75f, 0}, {1, .22f, .27f, 1}, {}},
		                                       {{-.75f, -.65f, 0}, {.12f, .9f, .72f, 1}, {}},
		                                       {{.75f, -.65f, 0}, {.2f, .4f, 1, 1}, {}}}};
		const std::array<std::uint32_t, 3> Indices{0, 1, 2};
		Draw.Vertices = Device.CreateBuffer(std::as_bytes(std::span(Vertices)));
		Draw.Indices = Device.CreateBuffer(std::as_bytes(std::span(Indices)));
		Draw.VertexStride = sizeof(FVertex);
		Draw.IndexCount = 3;
	}

	void Build(FRenderGraph& InGraph, const FRenderFrame& InFrame) override
	{
		FDrawPacket FrameDraw = Draw;
		const float TriangleScale = static_cast<float>(InFrame.Settings.TriangleScale);
		FrameDraw.Constants =
		    Scale({TriangleScale * static_cast<float>(InFrame.Size.Height) / static_cast<float>(InFrame.Size.Width),
		           TriangleScale, 1});
		FrameDraw.Scissor = {0, 0, static_cast<std::int32_t>(InFrame.Size.Width),
		                     static_cast<std::int32_t>(InFrame.Size.Height)};
		FColorPass Pass;
		Pass.Commands.Name = "Triangle";
		Pass.Commands.Draws.push_back(std::move(FrameDraw));
		InGraph.Add(std::move(Pass));
	}

	void Stop() noexcept override
	{
		Draw = {};
	}

private:
	FRhiDevice& Device;
	FShaderCompiler& Compiler;
	FTaskSystem& Tasks;
	FDrawPacket Draw;
};
} // namespace

void RegisterTrianglePlugin(FPluginRegistry& InRegistry, FRhiDevice& InDevice, FShaderCompiler& InCompiler,
                            FTaskSystem& InTasks)
{
	InRegistry.Add({"triangle",
	                {},
	                [&]
	                {
		                return std::make_unique<FTrianglePlugin>(InDevice, InCompiler, InTasks);
	                }});
}
} // namespace Hyperion
