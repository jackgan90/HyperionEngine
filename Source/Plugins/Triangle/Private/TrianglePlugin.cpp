#include "Hyperion/Triangle/TrianglePlugin.h"
#include "Hyperion/Renderer/RenderSession.h"
#include <algorithm>
#include <array>
#include <cstddef>

namespace Hyperion
{
namespace
{
FRenderResourceDesc PrepareTriangle(FShaderCompiler& InCompiler, EShaderFormat InFormat)
{
	FRenderResourceDesc Desc;
	FRenderMaterialDesc Material;
	FMaterialDescription Description;
	Description.Name = "Triangle";
	FMaterialPass Pass;
	Pass.Vertex = {"Triangle.hlsl", "VSMain"};
	Pass.Pixel = {"Triangle.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	auto Transform =
	    DeclareMaterialSemantic("Transform", "Engine.Object.WorldViewProjection", *GetStandardMaterialSemantics());
	Transform.Targets = {"DrawConstants.TransformMatrix"};
	Description.Parameters.push_back(std::move(Transform));
	const auto Definition = std::make_shared<const FMaterialDefinition>(std::move(Description));
	Material.Compiled = std::make_shared<const FCompiledMaterialDefinition>(
	    CompileMaterialDefinition(InCompiler, Definition, InFormat));
	Material.Surface = FMaterialInstance(Material.Compiled->Interface).Freeze();
	const std::array<FVertex, 3> Vertices{{{{0, .75f, 0}, {1, .22f, .27f, 1}, {}},
	                                       {{-.75f, -.65f, 0}, {.12f, .9f, .72f, 1}, {}},
	                                       {{.75f, -.65f, 0}, {.2f, .4f, 1, 1}, {}}}};
	FRenderGeometryDesc Geometry;
	const auto Bytes = std::as_bytes(std::span(Vertices));
	Geometry.Vertices.assign(Bytes.begin(), Bytes.end());
	Geometry.Indices = {0, 1, 2};
	Geometry.VertexStride = sizeof(FVertex);
	Geometry.Attributes = {{"POSITION", 0, EVertexFormat::Float3, offsetof(FVertex, Position)},
	                       {"COLOR", 0, EVertexFormat::Float4, offsetof(FVertex, Color)},
	                       {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FVertex, Uv)}};
	Geometry.Bounds = {{-.75f, -.65f, 0}, {.75f, .75f, 0}, true};
	Desc.Geometries.push_back(std::move(Geometry));
	Desc.Materials.push_back(std::move(Material));
	Desc.Sections.push_back({0, 0, 0, 3});
	return Desc;
}

class FTrianglePlugin final : public IScenePlugin
{
public:
	FTrianglePlugin(FRenderSession& InSession, IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks)
	    : Session(InSession), Compiler(InCompiler), Tasks(InTasks), Format(InDevice.GetCapabilities().ShaderFormat)
	{
	}

	void Start() override
	{
		Tasks.Require({EDomain::Main});
		static const auto Identity = std::make_shared<const int>(0);
		State.Resource = Session.GetResources().Request(Identity, 1, "TriangleVertex-v1",
		                                                [ShaderCompiler = &Compiler, ShaderFormat = Format]
		                                                {
			                                                return PrepareTriangle(*ShaderCompiler, ShaderFormat);
		                                                });
		State.bClipSpace = true;
		Binding = Session.GetScene().Create(State);
	}

	void Update(FRenderFrame& InFrame) override
	{
		Tasks.Require({EDomain::Main});
		const float TriangleScale = static_cast<float>(InFrame.Settings.TriangleScale);
		const auto World =
		    Scale({TriangleScale * float(std::max(1u, InFrame.Size.Height)) / std::max(1u, InFrame.Size.Width),
		           TriangleScale, 1});
		if (State.World.Values != World.Values)
		{
			State.World = World;
			++State.Revision;
			Session.GetScene().Update({{Binding.GetHandle(), State}});
		}
	}

	void Stop() noexcept override
	{
		Tasks.Require({EDomain::Main});
		Binding.Remove();
		State.Resource.reset();
	}

private:
	FRenderSession& Session;
	FShaderCompiler& Compiler;
	FTaskSystem& Tasks;
	EShaderFormat Format;
	FRenderPrimitiveState State;
	FRenderBinding Binding;
};
} // namespace

void RegisterTrianglePlugin(FPluginRegistry& InRegistry, FRenderSession& InSession, IRHIDevice& InDevice,
                            FShaderCompiler& InCompiler, FTaskSystem& InTasks)
{
	InRegistry.Add({"triangle",
	                {},
	                [&]
	                {
		                return std::make_unique<FTrianglePlugin>(InSession, InDevice, InCompiler, InTasks);
	                }});
}
} // namespace Hyperion
