#include "Hyperion/RHI/RHIDevice.h"
#include "Hyperion/Renderer/MaterialPacking.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <cmath>
#include <fstream>

using namespace Hyperion;

namespace
{
void WriteShader(const std::filesystem::path& InRoot)
{
	std::filesystem::create_directories(InRoot);
	std::ofstream(InRoot / "Packing.hlsl") << R"(
#ifndef NESTED_MATRICES
#define NESTED_MATRICES 1
#endif
struct FInner { float3 Vector; float Factor; bool bEnabled; float2 Pair; uint Tag; };
struct FOuter { float Leading; FInner Inner; int Signed; };
cbuffer Complex : register(b3, space1)
{
    FOuter Nested;
    FInner Samples[2];
    float Scalars[3];
    row_major float2x3 RowMatrix;
    column_major float3x2 ColumnMatrix;
    row_major float2x3 RowArray[2];
    column_major float3x2 ColumnArray[2];
    uint Bits;
    bool bFlag;
    float SignedZero;
    float Grid[2][3];
    bool Flags[2][3];
    FInner SampleGrid[2][2];
#if NESTED_MATRICES
    row_major float2x3 RowGrid[2][2];
    column_major float3x2 ColumnGrid[2][2];
#endif
};
float4 VSMain(float2 InPosition : POSITION) : SV_Position { return float4(InPosition, .5, 1); }
float4 PSMain() : SV_Target0
{
    float Red = Nested.Leading + Nested.Inner.Vector.z * Nested.Inner.Factor + Nested.Signed;
    Red += Nested.Inner.bEnabled ? Nested.Inner.Pair.y + Nested.Inner.Tag : 100;
    Red += Scalars[0] + 2 * Scalars[1] + 3 * Scalars[2];
    float Green = RowMatrix[0][2] + 2 * RowMatrix[1][0] + ColumnMatrix[2][1];
    Green += Samples[0].Vector.y + Samples[1].Pair.x + (Samples[1].bEnabled ? 100 : 0);
    float Blue = RowArray[0][1][2] + 2 * RowArray[1][0][1];
    Blue += ColumnArray[0][2][0] + 3 * ColumnArray[1][1][1] + Bits;
    [unroll] for (uint Row = 0; Row < 2; ++Row)
    {
        [unroll] for (uint Col = 0; Col < 3; ++Col)
        {
            Red += Grid[Row][Col] * (Row * 3 + Col + 1);
            Green += Flags[Row][Col] ? (1u << (Row * 3 + Col)) : 0;
        }
        [unroll] for (uint Col = 0; Col < 2; ++Col)
        {
            Blue += SampleGrid[Row][Col].Vector.y * (Row * 2 + Col + 1);
#if NESTED_MATRICES
            Blue += (RowGrid[Row][Col][1][2] + ColumnGrid[Row][Col][2][1]) * (Row * 2 + Col + 1);
#endif
        }
    }
    return float4(Red / 256, Green / 200, Blue / 1024, bFlag && asuint(SignedZero) == 0x80000000u ? 1 : 0);
}
)";
}

FMaterialValue Inner(float InVectorY, float InPairX, bool bInEnabled)
{
	FMaterialValue Value;
	Value.Type.Kind = EMaterialValueKind::Structure;
	Value.Type.MemberNames = {"Vector", "Factor", "bEnabled", "Pair", "Tag"};
	Value.Elements = {FMaterialValue::Float(FVec3{1, InVectorY, 3}), FMaterialValue::Float(2),
	                  FMaterialValue::Bool(bInEnabled), FMaterialValue::Float(FVec2{InPairX, 5}),
	                  FMaterialValue::Uint(7)};
	for (const auto& Element : Value.Elements)
	{
		Value.Type.Members.push_back(Element.Type);
	}
	Value.Validate();
	return Value;
}

FMaterialValue MatrixGrid(std::uint32_t InRows)
{
	std::vector<FMaterialValue> Rows;
	for (std::uint32_t Row = 0; Row < 2; ++Row)
	{
		std::vector<FMaterialValue> Columns;
		for (std::uint32_t Column = 0; Column < 2; ++Column)
		{
			std::array<float, 6> Values;
			for (std::uint32_t Index = 0; Index < Values.size(); ++Index)
			{
				Values[Index] = float((Row * 2 + Column) * 6 + Index + 1);
			}
			Columns.push_back(FMaterialValue::Floats(Values, InRows));
		}
		Rows.push_back(FMaterialValue::Array(std::move(Columns)));
	}
	return FMaterialValue::Array(std::move(Rows));
}

std::vector<FMaterialParameterEntry> GridValues()
{
	return {
	    {"Grid",
	     FMaterialValue::Array(
	         {FMaterialValue::Array({FMaterialValue::Float(1), FMaterialValue::Float(2), FMaterialValue::Float(3)}),
	          FMaterialValue::Array({FMaterialValue::Float(4), FMaterialValue::Float(5), FMaterialValue::Float(6)})})},
	    {"Flags", FMaterialValue::Array({FMaterialValue::Array({FMaterialValue::Bool(true), FMaterialValue::Bool(false),
	                                                            FMaterialValue::Bool(true)}),
	                                     FMaterialValue::Array({FMaterialValue::Bool(false), FMaterialValue::Bool(true),
	                                                            FMaterialValue::Bool(false)})})},
	    {"SampleGrid", FMaterialValue::Array({FMaterialValue::Array({Inner(1, 1, true), Inner(2, 2, true)}),
	                                          FMaterialValue::Array({Inner(3, 3, true), Inner(4, 4, true)})})},
	    {"RowGrid", MatrixGrid(2)},
	    {"ColumnGrid", MatrixGrid(3)}};
}

void SetValues(FMaterialInstance& InInstance)
{
	InInstance.Set("Pixel:Complex.Nested.Leading", FMaterialValue::Float(2));
	InInstance.Set("Pixel:Complex.Nested.Inner.Vector", FMaterialValue::Float(FVec3{1, 2, 3}));
	InInstance.Set("Pixel:Complex.Nested.Inner.Factor", FMaterialValue::Float(4));
	InInstance.Set("Pixel:Complex.Nested.Inner.bEnabled", FMaterialValue::Bool(true));
	InInstance.Set("Pixel:Complex.Nested.Inner.Pair", FMaterialValue::Float(FVec2{5, 6}));
	InInstance.Set("Pixel:Complex.Nested.Inner.Tag", FMaterialValue::Uint(7));
	InInstance.Set("Pixel:Complex.Nested.Signed", FMaterialValue::Int(-3));
	InInstance.Set("Samples", FMaterialValue::Array({Inner(4, 8, true), Inner(9, 6, false)}));
	InInstance.Set("Scalars", FMaterialValue::Array(
	                              {FMaterialValue::Float(1), FMaterialValue::Float(2), FMaterialValue::Float(3)}));
	const std::array<float, 6> First{1, 2, 3, 4, 5, 6};
	const std::array<float, 6> Second{7, 8, 9, 10, 11, 12};
	InInstance.Set("RowMatrix", FMaterialValue::Floats(First, 2));
	InInstance.Set("ColumnMatrix", FMaterialValue::Floats(Second, 3));
	InInstance.Set("RowArray",
	               FMaterialValue::Array({FMaterialValue::Floats(First, 2), FMaterialValue::Floats(Second, 2)}));
	InInstance.Set("ColumnArray",
	               FMaterialValue::Array({FMaterialValue::Floats(First, 3), FMaterialValue::Floats(Second, 3)}));
	InInstance.Set("Bits", FMaterialValue::Uint(13));
	InInstance.Set("bFlag", FMaterialValue::Bool(true));
	InInstance.Set("SignedZero", FMaterialValue::Float(-0.f));
	for (const auto& Entry : GridValues())
	{
		InInstance.Set(Entry.Name, Entry.Value);
	}
}

FDrawPacket PrepareDraw(IRHIDevice& InDevice, const FCompiledMaterialDefinition& InCompiled)
{
	FMaterialInstance Instance(InCompiled.Interface);
	SetValues(Instance);
	const auto Snapshot = Instance.Freeze();
	const auto& Program = InCompiled.GetPass();
	const auto Values = ResolveMaterialParameters(*Snapshot, {}, {}, {}, Program.ActiveParameters);
	std::vector<std::optional<FMaterialValue>> Indexed(Snapshot->Schema->GetParameters().size());
	for (const auto& Entry : Values)
	{
		Indexed[Snapshot->Schema->Find(Entry.Name).Index] = Entry.Value;
	}
	FDrawPacket Draw;
	const auto Layout = InDevice.CreateBindingLayout(DescribeMaterialLayout(Program));
	Draw.Bindings = InDevice.CreateBindingSet({Layout, {}});
	Draw.VertexStride = 8;
	Draw.Pipeline = InDevice.CreatePipeline(DescribeMaterialPipeline(
	    Program, Snapshot->Definition->GetPass(), Layout, {{"POSITION", 0, EVertexFormat::Float2, 0}},
	    Draw.VertexStride, ERHIPrimitiveTopology::TriangleList, {}));
	for (std::uint32_t Index = 0; Index < Program.Bindings.size(); ++Index)
	{
		const auto Data = PackMaterialConstants(Program.Bindings[Index], Indexed);
		const auto Extent = (Data.size() + 255) / 256 * 256;
		FBuffer Page = InDevice.CreateBuffer({Extent, BufferUsage(ERHIBufferUsage::Constant)});
		Draw.ConstantBindings.push_back({Index, InDevice.PublishConstantSlice(Page, 0, Data)});
	}
	const std::array<float, 6> Vertices{-1, -1, 3, -1, -1, 3};
	const std::array<std::uint32_t, 3> Indices{0, 1, 2};
	Draw.Vertices = InDevice.CreateBuffer(std::as_bytes(std::span(Vertices)));
	Draw.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	Draw.IndexCount = 3;
	Draw.Scissor = {0, 0, 64, 64};
	return Draw;
}

void RenderAndCheck(IRHISwapchain& InSwapchain, FDrawPacket InDraw)
{
	InSwapchain.BeginFrame({64, 64});
	FPassCommands Pass;
	Pass.Color = FColorAttachment{FRenderTarget::Backbuffer()};
	Pass.Name = "Complex reflected material constants";
	Pass.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::Present, EResourceState::RenderTarget}};
	Pass.Color->Actions.Load = EAttachmentLoad::Clear;
	Pass.Draws = {std::move(InDraw)};
	FPassCommands Present;
	Present.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::RenderTarget, EResourceState::Present}};
	const std::array Lists{InSwapchain.Record(0, Pass), InSwapchain.Record(1, Present)};
	const FImage Image = InSwapchain.EndFrame(Lists, false, true);
	const std::size_t Center = (32 * Image.Width + 32) * 4;
	// Independent arithmetic over the source values, never over packer output or reflected offsets.
	const float Red = (2 + 3 * 4 - 3 + 6 + 7 + 1 + 2 * 2 + 3 * 3 + 91) / 256.F;
	const float Green = (3 + 2 * 4 + 12 + 4 + 6 + 21) / 200.F;
	const float Blue = (6 + 2 * 8 + 5 + 3 * 10 + 13 + 30 + 180 + 180) / 1024.F;
	HYP_CHECK(std::abs(Image.Rgba[Center] - Red) < .01F);
	HYP_CHECK(std::abs(Image.Rgba[Center + 1] - Green) < .01F);
	HYP_CHECK(std::abs(Image.Rgba[Center + 2] - Blue) < .01F);
	HYP_CHECK(Image.Rgba[Center + 3] > .99F);
}

void CheckInvalidGridSchemas(FShaderCompiler& InCompiler, const FMaterialDescription& InDescription)
{
	for (const bool bWrongCount : {false, true})
	{
		auto Description = InDescription;
		Description.Passes.front().Vertex.Defines = {{"NESTED_MATRICES", "0"}};
		Description.Passes.front().Pixel.Defines = {{"NESTED_MATRICES", "0"}};
		auto& Grid = Description.Parameters.front().Type;
		if (bWrongCount)
		{
			++Grid.ArrayCount;
		}
		else
		{
			Grid.Members.front().Members.front().Scalar = EMaterialScalar::Uint;
		}
		const auto Definition = std::make_shared<const FMaterialDefinition>(Description);
		for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
		{
			bool bRejected{};
			try
			{
				CompileMaterialDefinition(InCompiler, Definition, Format);
			}
			catch (const std::invalid_argument&)
			{
				bRejected = true;
			}
			HYP_CHECK(bRejected);
		}
	}
}
} // namespace

void RunMaterialPackingGpuTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	const auto Root = std::filesystem::absolute("material-packing-gpu/source");
	WriteShader(Root);
	const auto Cache = std::filesystem::path("material-packing-gpu/cache") /
	                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	FShaderCompiler Compiler(Root, Cache);
	FMaterialDescription Description;
	Description.Name = "Complex constants GPU oracle";
	FMaterialPass Pass;
	Pass.Vertex = {"Packing.hlsl", "VSMain"};
	Pass.Pixel = {"Packing.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	for (const auto& Entry : GridValues())
	{
		FMaterialParameterDeclaration Parameter;
		Parameter.Name = Entry.Name;
		Parameter.Type = Entry.Value.Type;
		Description.Parameters.push_back(std::move(Parameter));
	}
	auto Definition = std::make_shared<const FMaterialDefinition>(Description);
	for (bool bHot : {false, true})
	{
		const auto Compiled = CompileMaterialDefinition(Compiler, Definition, EShaderFormat::Dxil);
		HYP_CHECK(Compiled.GetPass().Vertex.bCacheHit == bHot && Compiled.GetPass().Pixel.bCacheHit == bHot);
		RenderAndCheck(InSwapchain, PrepareDraw(InDevice, Compiled));
	}
	for (const auto Format : {EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		// This DXC build rejects direct multidimensional matrix arrays when generating SPIR-V.
		// DXIL above validates those real pixels; the other targets validate bool/float/struct grids.
		auto CrossDescription = Description;
		CrossDescription.Passes.front().Vertex.Defines = {{"NESTED_MATRICES", "0"}};
		CrossDescription.Passes.front().Pixel.Defines = {{"NESTED_MATRICES", "0"}};
		const auto CrossDefinition = std::make_shared<const FMaterialDefinition>(CrossDescription);
		const auto First = CompileMaterialDefinition(Compiler, CrossDefinition, Format);
		FMaterialInstance Instance(First.Interface);
		SetValues(Instance);
		const auto Hot = CompileMaterialDefinition(Compiler, CrossDefinition, Format);
		HYP_CHECK(Hot.GetPass().Pixel.bCacheHit && Hot.GetPass().Pixel.Bindings == First.GetPass().Pixel.Bindings);
	}
	CheckInvalidGridSchemas(Compiler, Description);
	HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
}
