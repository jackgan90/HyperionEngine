#include "Hyperion/Renderer/MaterialPreparation.h"
#include "Support/TestSupport.h"
#include <fstream>
#include <iostream>

using namespace Hyperion;

void RunMaterialBlockTests();
void RunMaterialProviderTests();

namespace
{
void ExpectError(const std::function<void()>& InAction, std::string_view InText)
{
	try
	{
		InAction();
	}
	catch (const std::exception& Error)
	{
		HYP_CHECK(std::string_view(Error.what()).find(InText) != std::string_view::npos);
		return;
	}
	throw std::runtime_error("Expected material binding error: " + std::string(InText));
}

FMaterialDescription MakeDescription()
{
	FMaterialDescription Description;
	Description.Name = "Reflected custom material";
	FMaterialPass Pass;
	Pass.Vertex = {"Interface.hlsl", "VSMain"};
	Pass.Pixel = {"Interface.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	return Description;
}

void CheckPreparation()
{
	const auto Root = std::filesystem::absolute("material-binding-test/source");
	std::filesystem::create_directories(Root);
	{
		std::ofstream File(Root / "Interface.hlsl");
		File << R"(
cbuffer Shared : register(b0, space1) { float4 Tint; };
cbuffer Surface : register(b3)
{
#if PARAMETER_INT
    int Strength;
#else
    float Strength;
#endif
    float Inactive;
};
float4 VSMain(float3 InPosition : POSITION) : SV_Position { return float4(InPosition + Tint.xyz, 1); }
float4 PSMain() : SV_Target0 { return Tint * Strength; }
)";
	}
	FTaskSystem Tasks(1, 1);
	auto Compiler = std::make_shared<FShaderCompiler>(Root, "material-binding-test/cache");
	auto Definition = std::make_shared<const FMaterialDefinition>(MakeDescription());
	FMaterialInstance Unprepared(Definition);
	ExpectError(
	    [&]
	    {
		    Unprepared.Find("Strength");
	    },
	    "InterfaceNotReady");
	const auto Request = PrepareMaterialDefinition(Tasks, Compiler, Definition, EShaderFormat::Dxil);
	const auto Compiled = Request.Get(Tasks);
	FMaterialInstance Instance(Compiled->Interface);
	Instance.Set("Strength", FMaterialValue::Float(0.8F));
	ExpectError(
	    [&]
	    {
		    Instance.Find("Tint");
	    },
	    "Ambiguous");
	Instance.Set("Vertex:Shared.Tint", FMaterialValue::Float(FVec4{0, 0, 0, 1}));
	Instance.Set("Pixel:Shared.Tint", FMaterialValue::Float(FVec4{1, 1, 1, 1}));
	HYP_CHECK(Compiled->GetPass().Bindings.size() == 3);
	const auto Snapshot = Instance.Freeze();
	FMaterialDescription Description = MakeDescription();
	FMaterialParameterDeclaration Color;
	Color.Name = "Color";
	Color.Targets = {"Shared.Tint"};
	Color.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float, 4);
	Description.Parameters.push_back(Color);
	FMaterialParameterDeclaration Inactive;
	Inactive.Name = "Pixel:Surface.Inactive";
	Inactive.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float);
	Description.Parameters.push_back(Inactive);
	Definition = std::make_shared<const FMaterialDefinition>(Description);
	const auto Merged = PrepareMaterialDefinition(Tasks, Compiler, Definition, EShaderFormat::Dxil).Get(Tasks);
	HYP_CHECK(Merged->GetPass().Bindings.size() == 2 && Merged->GetPass().Bindings[0].Stages == 3);
	FMaterialInstance Named(Merged->Interface);
	HYP_CHECK(Named.Find("Color").Index == Named.Find("Vertex:Shared.Tint").Index);
	HYP_CHECK(Named.Find("Color").Index == Named.Find("Pixel:Shared.Tint").Index);
	HYP_CHECK(Named.Set("Inactive", FMaterialValue::Float(2)) == EMaterialWriteResult::Inactive);
	ExpectError(
	    [&]
	    {
		    Named.Find("Typo");
	    },
	    "UnknownParameter");
	FCancellationToken Cancel;
	Cancel.Cancel();
	const auto Cancelled = PrepareMaterialDefinition(Tasks, Compiler, Definition, EShaderFormat::Dxil, {}, Cancel);
	ExpectError(
	    [&]
	    {
		    Cancelled.Get(Tasks);
	    },
	    "canceled");
	ExpectError(
	    [&]
	    {
		    Cancelled.GetReady();
	    },
	    "canceled");
	const std::vector<FMaterialVariantRequest> Variants{{"Forward", "Float", {{"PARAMETER_INT", "0"}}},
	                                                    {"Forward", "Int", {{"PARAMETER_INT", "1"}}}};
	const auto Conflict = PrepareMaterialDefinition(Tasks, Compiler, Definition, EShaderFormat::Dxil, Variants);
	ExpectError(
	    [&]
	    {
		    Conflict.Get(Tasks);
	    },
	    "type conflict");
	HYP_CHECK(PrepareMaterialDefinition(Tasks, Compiler, Definition, EShaderFormat::Dxil)
	              .Get(Tasks)
	              ->Interface.Schema->IsPrepared());
	HYP_CHECK(Instance.Freeze() == Snapshot);
	Tasks.Shutdown();
}

void CheckTextureComponentRejection()
{
	const auto Root = std::filesystem::absolute("material-binding-test/source");
	std::ofstream(Root / "Integer.hlsl") << R"(
Texture2D<uint4> Image : register(t0);
float4 VSMain(float3 InPosition : POSITION) : SV_Position { return float4(InPosition,1); }
float4 PSMain() : SV_Target0 { return float4(Image.Load(int3(0,0,0))); }
)";
	FShaderCompiler Compiler(Root, "material-binding-test/cache");
	auto Description = MakeDescription();
	Description.Passes.front().Vertex.Path = "Integer.hlsl";
	Description.Passes.front().Pixel.Path = "Integer.hlsl";
	const auto Definition = std::make_shared<const FMaterialDefinition>(Description);
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		ExpectError(
		    [&]
		    {
			    CompileMaterialDefinition(Compiler, Definition, Format);
		    },
		    "component type");
	}
}

void CheckBindingConflicts()
{
	FMaterialProgramBinding A;
	A.Resource.Kind = EBindingKind::Texture;
	A.Resource.Name = "A";
	A.Resource.Count = 2;
	A.Stages = 1;
	A.ResourceParameter = 0;
	FMaterialProgramBinding B = A;
	B.Resource.Name = "B";
	B.Stages = 2;
	B.ResourceParameter = 1;
	HYP_CHECK(MergeMaterialBindings({A, B}).size() == 2);
	B.Stages = 1;
	ExpectError(
	    [&]
	    {
		    MergeMaterialBindings({A, B});
	    },
	    "Overlapping");
	B.Stages = 2;
	FMaterialProgramBinding Both = A;
	Both.Stages = 3;
	ExpectError(
	    [&]
	    {
		    MergeMaterialBindings({A, B, Both});
	    },
	    "Overlapping");
}
} // namespace

int main()
{
	try
	{
		CheckPreparation();
		CheckBindingConflicts();
		CheckTextureComponentRejection();
		RunMaterialBlockTests();
		RunMaterialProviderTests();
		std::cout << "PASS: material interface preparation and stage bindings\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
