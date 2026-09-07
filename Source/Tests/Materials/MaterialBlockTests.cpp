#include "Hyperion/Renderer/MaterialBlocks.h"
#include "Support/TestSupport.h"
#include <fstream>

using namespace Hyperion;

void RunMaterialBlockTests()
{
	const auto Root = std::filesystem::absolute("material-block-test/source");
	std::filesystem::create_directories(Root);
	std::filesystem::copy_file(std::filesystem::path(HYP_SOURCE_DIR) / "shaders/MaterialBlocks.hlsli",
	                           Root / "MaterialBlocks.hlsli", std::filesystem::copy_options::overwrite_existing);
	std::ofstream(Root / "Blocks.hlsl") << R"(
#define HYP_MATERIAL_VIEW_V1
#define HYP_MATERIAL_OBJECT_V1
#define HYP_MATERIAL_SURFACE_V1
#define HYP_MATERIAL_SCENE_V1
)" << "#include \"MaterialBlocks.hlsli\"\n"
	                                    << R"(
float4 VSMain(float3 InPosition : POSITION) : SV_Position
{
    return mul(ViewProjection, mul(World, float4(InPosition * Normal[0][0], 1)));
}
float4 PSMain() : SV_Target0
{
    float3 Color = BaseColor.rgb + Emissive + AmbientColor + MainLightColor * MainLightDirection.z;
    return float4(Color + CameraPosition * OrientationSign + (bUnlit ? 1 : 0), 1);
}
)";
	FShaderCompiler Compiler(Root, "material-block-test/cache");
	FMaterialDescription Description;
	Description.Name = "Canonical blocks";
	FMaterialPass Pass;
	Pass.Vertex = {"Blocks.hlsl", "VSMain"};
	Pass.Pixel = {"Blocks.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	const auto Definition = std::make_shared<const FMaterialDefinition>(Description);
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const auto Prepared = CompileMaterialDefinition(Compiler, Definition, Format);
		const auto& Program = Prepared.GetPass();
		HYP_CHECK(Program.Bindings.size() == 4);
		HYP_CHECK(Prepared.Interface.Schema->GetParameters().size() == 24);
		for (const auto& Binding : Program.Bindings)
		{
			if (Binding.Resource.Name == "HyperionViewV1")
			{
				HYP_CHECK(Binding.Stages == 3 && Binding.Resource.ByteSize == 80 && Binding.Members.size() == 2);
			}
			if (Binding.Resource.Name == "HyperionObjectV1")
			{
				HYP_CHECK(Binding.Stages == 3 && Binding.Resource.ByteSize == 144 && Binding.Members.size() == 3);
			}
		}
	}
	FShaderBinding Invalid;
	Invalid.Name = "HyperionViewV1";
	Invalid.Kind = EBindingKind::UniformBuffer;
	Invalid.ByteSize = 80;
	FShaderMember Camera;
	Camera.Name = "CameraPosition";
	Camera.Offset = 0;
	Camera.Columns = 3;
	Camera.Size = 12;
	Invalid.Members.push_back(Camera);
	bool bRejected = false;
	try
	{
		NormalizeStandardMaterialBlock(Invalid);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
