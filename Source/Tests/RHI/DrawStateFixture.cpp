#include "RHI/DrawStateFixture.h"
#include "Support/TestSupport.h"
#include <cmath>

namespace Hyperion::D3D12Private
{
void CheckNativeDrawPlanFixture(IRHIDevice& InDevice, const FBuffer& InAnchor, const FDrawPacket& InA,
                                const FDrawPacket& InB, const FDrawPacket& InA2);
void CheckNativeDrawPlanLifetime(IRHIDevice& InDevice, FDrawPacket InDraw);
} // namespace Hyperion::D3D12Private

namespace Hyperion::Tests
{
namespace
{
FResourceBindingLayout MakeLayout(IRHIDevice& InDevice, bool bInAlternate)
{
	const FResourceBindingSlot Constant{ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Pixel, 0, 0, 1, 16};
	const FResourceBindingSlot Texture{ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel, 0};
	const FResourceBindingSlot Sampler{ERHIBindingKind::Sampler, ERHIShaderVisibility::Pixel, 0};
	return InDevice.CreateBindingLayout(
	    {bInAlternate ? std::vector{Texture, Sampler, Constant} : std::vector{Constant, Texture, Sampler}});
}

FBuffer MakeVertices(IRHIDevice& InDevice, bool bInFullScreen, bool bInPadded)
{
	const std::array<FVec2, 3> Positions = bInFullScreen
	                                           ? std::array<FVec2, 3>{{{-1, -1}, {3, -1}, {-1, 3}}}
	                                           : std::array<FVec2, 3>{{{0, .005f}, {-.005f, -.005f}, {.005f, -.005f}}};
	if (bInPadded)
	{
		const std::array<FVec4, 3> Padded{{{Positions[0].X, Positions[0].Y, 17, 23},
		                                   {Positions[1].X, Positions[1].Y, 31, 47},
		                                   {Positions[2].X, Positions[2].Y, 53, 67}}};
		return InDevice.CreateBuffer(std::as_bytes(std::span(Padded)));
	}
	return InDevice.CreateBuffer(std::as_bytes(std::span(Positions)));
}

FDrawPacket MakeDraw(IRHIDevice& InDevice, const FPipelineDesc& InPipeline, const FTexture& InTexture,
                     const FSampler& InSampler, const FBufferSlice& InTint, bool bInFullScreen, bool bInAlternate)
{
	FDrawPacket Result;
	Result.Pipeline = InDevice.CreatePipeline(InPipeline);
	Result.Vertices = MakeVertices(InDevice, bInFullScreen, bInAlternate);
	const std::vector<std::uint32_t> Indices = bInAlternate ? std::vector<std::uint32_t>{1, 1, 1, 1, 2, 3, 1, 1, 1}
	                                                        : std::vector<std::uint32_t>{1, 1, 1, 1, 2, 3};
	Result.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	Result.VertexStride = bInAlternate ? 16 : 8;
	Result.IndexCount = 3;
	Result.FirstIndex = 3;
	Result.VertexOffset = -1;
	Result.InstanceCount = bInAlternate ? 3 : 2;
	Result.ConstantBindings = {{bInAlternate ? 2U : 0U, InTint}};
	Result.Bindings = InDevice.CreateBindingSet(
	    {InPipeline.Layout, {{bInAlternate ? 0U : 1U, {InTexture}}, {bInAlternate ? 1U : 2U, {InSampler}}}});
	Result.DynamicState.StencilReference = bInAlternate ? 19 : 7;
	Result.DynamicState.BlendConstants =
	    bInAlternate ? std::array{.75f, .25f, .5f, 1.f} : std::array{.25f, .5f, .75f, 1.f};
	Result.Scissor = bInFullScreen ? (bInAlternate ? FRect{32, 0, 64, 64} : FRect{-7, -11, 32, 64})
	                               : (bInAlternate ? FRect{0, 0, 1439, 899} : FRect{-7, -11, 1440, 900});
	return Result;
}

struct FExpectedBinds
{
	std::uint64_t Root{};
	std::uint64_t Heap{};
	std::uint64_t Constant{};
	std::uint64_t Table{};
	std::uint64_t Pipeline{};
	std::uint64_t Geometry{};
	std::uint64_t Dynamic{};
};

void CheckBindDelta(const FDeviceStats& InBefore, const FDeviceStats& InAfter, const FExpectedBinds& InExpected)
{
	HYP_CHECK(InAfter.GraphicsRootBinds - InBefore.GraphicsRootBinds == InExpected.Root);
	HYP_CHECK(InAfter.GraphicsHeapBinds - InBefore.GraphicsHeapBinds == InExpected.Heap);
	HYP_CHECK(InAfter.GraphicsConstantBinds - InBefore.GraphicsConstantBinds == InExpected.Constant);
	HYP_CHECK(InAfter.GraphicsTableBinds - InBefore.GraphicsTableBinds == InExpected.Table);
	HYP_CHECK(InAfter.GraphicsPipelineBinds - InBefore.GraphicsPipelineBinds == InExpected.Pipeline);
	HYP_CHECK(InAfter.GraphicsGeometryBinds - InBefore.GraphicsGeometryBinds == InExpected.Geometry);
	HYP_CHECK(InAfter.GraphicsDynamicBinds - InBefore.GraphicsDynamicBinds == InExpected.Dynamic);
}

struct FPixelExpectation
{
	std::uint32_t X{};
	std::uint32_t Y{};
	FVec4 Color;
};

void CheckPixels(const FImage& InImage, std::span<const FPixelExpectation> InExpected)
{
	HYP_CHECK(InImage.Width == 64 && InImage.Height == 64);
	for (const auto& Pixel : InExpected)
	{
		const auto Offset = (Pixel.Y * InImage.Width + Pixel.X) * 4;
		const std::array Values{Pixel.Color.X, Pixel.Color.Y, Pixel.Color.Z, Pixel.Color.W};
		for (std::size_t Component = 0; Component < Values.size(); ++Component)
		{
			const float Expected = Values[Component];
			if (Expected == 0 || Expected == 1)
			{
				HYP_CHECK(InImage.Rgba.at(Offset + Component) == Expected);
			}
			else
			{
				HYP_CHECK(std::abs(InImage.Rgba.at(Offset + Component) - Expected) <= 1.f / 255.f + 1e-6f);
			}
		}
	}
}

FImage Render(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, const std::shared_ptr<const FPassCommands>& InCommands,
              bool bInOwned, const FExpectedBinds& InExpected)
{
	InSwapchain.BeginFrame({64, 64});
	try
	{
		const auto Before = InDevice.Statistics();
		const auto Recorded = bInOwned ? InSwapchain.RecordOwned(0, InCommands) : InSwapchain.Record(0, *InCommands);
		CheckBindDelta(Before, InDevice.Statistics(), InExpected);
		FPassCommands Present;
		Present.Name = "Draw-state present";
		Present.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::RenderTarget, EResourceState::Present}};
		const std::array Lists{Recorded, InSwapchain.Record(1, Present)};
		return InSwapchain.EndFrame(Lists, false, true);
	}
	catch (...)
	{
		InSwapchain.CancelFrame();
		throw;
	}
}

void CheckModes(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, FPassCommands InCommands,
                const FExpectedBinds& InBinds, std::span<const FPixelExpectation> InPixels)
{
	InCommands.ShareDraws();
	const auto Commands = std::make_shared<const FPassCommands>(std::move(InCommands));
	const auto Ordinary = Render(InDevice, InSwapchain, Commands, false, InBinds);
	CheckPixels(Ordinary, InPixels);
	for (unsigned Recording = 0; Recording < 4; ++Recording)
	{
		// The same context-0 stream exercises first owned, build, and two subsequent reuse recordings.
		const auto Image = Render(InDevice, InSwapchain, Commands, true, InBinds);
		CheckPixels(Image, InPixels);
		HYP_CHECK(Image.Rgba == Ordinary.Rgba);
	}
}

void CheckSwitching(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, const FDrawStateFixture& InFixture)
{
	const std::array Split{FPixelExpectation{16, 32, {1, 0, 0, 1}}, FPixelExpectation{48, 32, {0, 1, 0, 1}}};
	CheckModes(InDevice, InSwapchain, MakeDrawStateCommands({InFixture.A, InFixture.B}), {2, 1, 2, 4, 2, 5, 6}, Split);
	const std::array Red{FPixelExpectation{16, 32, {1, 0, 0, 1}}, FPixelExpectation{48, 32, {0, 0, 0, 1}}};
	CheckModes(InDevice, InSwapchain, MakeDrawStateCommands({InFixture.A, InFixture.A}), {1, 1, 1, 2, 1, 3, 3}, Red);
	const std::array Green{FPixelExpectation{16, 32, {0, 1, 0, 1}}, FPixelExpectation{48, 32, {0, 0, 0, 1}}};
	CheckModes(InDevice, InSwapchain, MakeDrawStateCommands({InFixture.A, InFixture.A2}), {1, 1, 2, 4, 1, 3, 3}, Green);
	CheckModes(InDevice, InSwapchain, MakeDrawStateCommands({}), {},
	           std::array{FPixelExpectation{32, 32, {0, 0, 0, 1}}});
	auto Clipped = InFixture.A;
	Clipped.Scissor = {-7, -11, 24, 40};
	const std::array Clip{FPixelExpectation{8, 8, {1, 0, 0, 1}}, FPixelExpectation{32, 8, {0, 0, 0, 1}},
	                      FPixelExpectation{8, 48, {0, 0, 0, 1}}};
	CheckModes(InDevice, InSwapchain, MakeDrawStateCommands({Clipped}), {1, 1, 1, 2, 1, 3, 3}, Clip);
}

void CheckOldStream(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, const FDrawStateFixture& InFixture)
{
	auto Source = MakeDrawStateCommands({InFixture.A, InFixture.B});
	Source.ShareDraws();
	const auto Original = std::make_shared<const FPassCommands>(std::move(Source));
	const FExpectedBinds Binds{2, 1, 2, 4, 2, 5, 6};
	const auto Before = Render(InDevice, InSwapchain, Original, true, Binds);
	Render(InDevice, InSwapchain, Original, true, Binds);
	auto NewSource = *Original;
	NewSource.MaterializeDraws();
	NewSource.Draws[0].FirstIndex = 0; // The legal prefix is degenerate, leaving the left side black.
	NewSource.ShareDraws();
	const auto Changed = std::make_shared<const FPassCommands>(std::move(NewSource));
	const std::array Pixels{FPixelExpectation{16, 32, {0, 0, 0, 1}}, FPixelExpectation{48, 32, {0, 1, 0, 1}}};
	CheckPixels(Render(InDevice, InSwapchain, Changed, true, Binds), Pixels);
	CheckPixels(Render(InDevice, InSwapchain, Changed, true, Binds), Pixels);
	HYP_CHECK(Render(InDevice, InSwapchain, Original, true, Binds).Rgba == Before.Rgba);
	HYP_CHECK(Original->GetDraws()[0].FirstIndex == 3 && Changed->GetDraws()[0].FirstIndex == 0);
}

void CheckBlend(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, const FDrawStateFixture& InFixture)
{
	auto Description = InFixture.PipelineA;
	Description.State.bBlend = true;
	Description.State.SourceRgb = ERHIBlendFactor::Constant;
	Description.State.DestinationRgb = ERHIBlendFactor::InverseConstant;
	auto Left = InFixture.A;
	Left.Pipeline = InDevice.CreatePipeline(Description);
	Left.InstanceCount = 1;
	Left.ConstantBindings = {{0, InFixture.WhiteTint}};
	Left.Scissor = {0, 0, 32, 64};
	auto Right = Left;
	Right.Scissor = {32, 0, 64, 64};
	Right.DynamicState.BlendConstants = {.75f, .25f, .5f, 1};
	const std::array Pixels{FPixelExpectation{16, 32, {.25f, .5f, .75f, 1}},
	                        FPixelExpectation{48, 32, {.75f, .25f, .5f, 1}}};
	CheckModes(InDevice, InSwapchain, MakeDrawStateCommands({Left, Right}), {1, 1, 1, 2, 1, 3, 5}, Pixels);
}

void CheckStencil(IRHIDevice& InDevice, const FDrawStateFixture& InFixture)
{
	FWindow Window("Native draw stencil fixture", {64, 64}, true);
	auto Swapchain = InDevice.CreateSwapchain({Window.Surface(), Window.PixelSize(), ERHIDepthFormat::D32S8});
	auto Description = InFixture.PipelineA;
	Description.Target.Depth = ERHIDepthFormat::D32S8;
	Description.State.bStencil = true;
	Description.State.ColorWriteMask = 0;
	Description.State.FrontStencil.Pass = ERHIStencilOp::Replace;
	Description.State.BackStencil.Pass = ERHIStencilOp::Replace;
	auto Write = InFixture.A;
	Write.Pipeline = InDevice.CreatePipeline(Description);
	Write.InstanceCount = 1;
	Write.Scissor = {0, 0, 64, 64};
	Description.State.ColorWriteMask = 15;
	Description.State.StencilWriteMask = 0;
	Description.State.FrontStencil = {ERHICompare::Equal};
	Description.State.BackStencil = {ERHICompare::Equal};
	auto Blue = Write;
	Blue.Pipeline = InDevice.CreatePipeline(Description);
	const auto Page = InDevice.CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
	const FVec4 Color{0, 0, 1, 1};
	Blue.ConstantBindings = {{0, InDevice.PublishConstantSlice(Page, 0, std::as_bytes(std::span(&Color, 1)))}};
	auto RejectedRed = Blue;
	RejectedRed.ConstantBindings = InFixture.A.ConstantBindings;
	RejectedRed.DynamicState.StencilReference = 2;
	CheckModes(InDevice, *Swapchain, MakeDrawStateCommands({Write, Blue, RejectedRed}, true), {1, 1, 3, 2, 2, 3, 4},
	           std::array{FPixelExpectation{32, 32, {0, 0, 1, 1}}});
	Swapchain->WaitIdle();
}
} // namespace

FDrawStateFixture::FDrawStateFixture(IRHIDevice& InDevice, bool bInFullScreen)
{
	const auto Root = std::filesystem::path(HYP_SOURCE_DIR) / "Source/Tests/Shaders";
	FShaderCompiler Compiler(Root, std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache/native-draw-state");
	PipelineA.Vertex = Compiler.Compile("NativeDrawState.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
	PipelineA.Pixel = Compiler.Compile("NativeDrawState.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
	PipelineA.Attributes = {{"POSITION", 0, EVertexFormat::Float2, 0}};
	PipelineA.VertexStride = 8;
	PipelineA.Layout = MakeLayout(InDevice, false);
	auto PipelineB = PipelineA;
	PipelineB.VertexStride = 16;
	PipelineB.Layout = MakeLayout(InDevice, true);
	const auto White = InDevice.CreateTexture({1, 1, EColorSpace::Linear, {1, 1, 1, 1}});
	const auto Green = InDevice.CreateTexture({1, 1, EColorSpace::Linear, {0, 1, 0, 1}});
	FSamplerDesc Sampler;
	Sampler.bMinLinear = false;
	Sampler.bMagLinear = false;
	Sampler.bMipLinear = false;
	Sampler.bMipmapped = false;
	const auto Repeat = InDevice.CreateSampler(Sampler);
	Sampler.U = ERHIAddressMode::Clamp;
	Sampler.V = ERHIAddressMode::Clamp;
	Sampler.W = ERHIAddressMode::Clamp;
	const auto Clamp = InDevice.CreateSampler(Sampler);
	Page = InDevice.CreateBuffer({512, BufferUsage(ERHIBufferUsage::Constant)});
	const FVec4 Red{1, 0, 0, 1};
	const FVec4 Untinted{1, 1, 1, 1};
	const auto RedTint = InDevice.PublishConstantSlice(Page, 0, std::as_bytes(std::span(&Red, 1)));
	WhiteTint = InDevice.PublishConstantSlice(Page, 256, std::as_bytes(std::span(&Untinted, 1)));
	A = MakeDraw(InDevice, PipelineA, White, Repeat, RedTint, bInFullScreen, false);
	B = MakeDraw(InDevice, PipelineB, Green, Clamp, WhiteTint, bInFullScreen, true);
	A2 = A;
	A2.ConstantBindings = {{0, WhiteTint}};
	A2.Bindings = InDevice.CreateBindingSet({PipelineA.Layout, {{1, {Green}}, {2, {Clamp}}}});
	InDevice.WaitIdle();
}

FPassCommands MakeDrawStateCommands(std::vector<FDrawPacket> InDraws, bool bInStencil)
{
	FPassCommands Result;
	Result.Name = "Native draw-state fixture";
	Result.Color = FColorAttachment{FRenderTarget::Backbuffer()};
	Result.Color->Actions.Load = EAttachmentLoad::Clear;
	Result.Color->Clear = {0, 0, 0, 1};
	Result.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::Present, EResourceState::RenderTarget}};
	Result.Draws = std::move(InDraws);
	if (bInStencil)
	{
		Result.DepthStencil = FDepthStencilAttachment{FRenderTarget::FrameDepth(), ERHIDepthFormat::D32S8,
		                                              FAttachmentActions{EAttachmentLoad::Clear},
		                                              FAttachmentActions{EAttachmentLoad::Clear}};
	}
	return Result;
}

void CheckNativeDrawStateFixtures(IRHIDevice& InDevice)
{
	const FDrawStateFixture Fixture(InDevice, true);
	D3D12Private::CheckNativeDrawPlanFixture(InDevice, Fixture.A.Vertices, Fixture.A, Fixture.B, Fixture.A2);
	D3D12Private::CheckNativeDrawPlanLifetime(InDevice, Fixture.A);
	FWindow Window("Native draw-state fixture", {64, 64}, true);
	auto Swapchain = InDevice.CreateSwapchain({Window.Surface(), Window.PixelSize()});
	CheckSwitching(InDevice, *Swapchain, Fixture);
	CheckBlend(InDevice, *Swapchain, Fixture);
	CheckOldStream(InDevice, *Swapchain, Fixture);
	Swapchain->WaitIdle();
	CheckStencil(InDevice, Fixture);
	InDevice.WaitIdle();
	HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
}
} // namespace Hyperion::Tests
