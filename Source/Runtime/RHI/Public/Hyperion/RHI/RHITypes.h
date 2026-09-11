#pragma once
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/RHI/RHIBindings.h"
#include "Hyperion/RHI/RHIGraphicsState.h"
#include "Hyperion/RHI/RHIResources.h"
#include "Hyperion/Shaders/ShaderCompiler.h"
#include <optional>
#include <span>
#include <stdexcept>

namespace Hyperion
{
enum class ERHIAddressMode
{
	Repeat,
	Clamp,
	Mirror,
	Border,
	MirrorOnce
};

struct FSamplerDesc
{
	ERHIAddressMode U = ERHIAddressMode::Repeat;
	ERHIAddressMode V = ERHIAddressMode::Repeat;
	bool bMinLinear = true;
	bool bMagLinear = true;
	bool bMipLinear = true;
	bool bMipmapped = true;
	ERHIAddressMode W = ERHIAddressMode::Repeat;
	float MipLodBias{};
	float MinLod{};
	float MaxLod = 3.402823466e+38F;
	std::uint32_t MaxAnisotropy = 1;
	std::array<float, 4> BorderColor{};
	bool bComparison{};
	ERHICompare Compare = ERHICompare::LessEqual;
	bool operator==(const FSamplerDesc&) const = default;
};

struct FDepthTextureDesc
{
	std::uint32_t Width = 1;
	std::uint32_t Height = 1;
	float ClearDepth = 1;
};

struct FColorTextureDesc
{
	std::uint32_t Width = 1;
	std::uint32_t Height = 1;
	ERHIColorFormat Format = ERHIColorFormat::Rgba16Float;
	FVec4 Clear;
};

struct FTextureMip
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::vector<std::uint8_t> Rgba;
};

struct FTextureDesc
{
	bool bSrgb{};
	std::vector<FTextureMip> Mips;
};
enum class EVertexFormat
{
	Float2,
	Float3,
	Float4,
	Unorm8x4,
	Float,
	Int,
	Int2,
	Int3,
	Int4,
	Uint,
	Uint2,
	Uint3,
	Uint4
};

struct FVertexAttribute
{
	std::string Semantic;
	std::uint32_t SemanticIndex{};
	EVertexFormat Format;
	std::uint32_t Offset{};
	bool operator==(const FVertexAttribute&) const = default;
};

struct FPipelineDesc
{
	FShaderArtifact Vertex;
	FShaderArtifact Pixel;
	std::vector<FVertexAttribute> Attributes;
	FResourceBindingLayout Layout;
	FGraphicsState State;
	FGraphicsTarget Target;
	ERHIPrimitiveTopology Topology = ERHIPrimitiveTopology::TriangleList;
	std::uint32_t VertexStride{};
};

struct FRect
{
	std::int32_t Left{};
	std::int32_t Top{};
	std::int32_t Right{};
	std::int32_t Bottom{};
};

struct FDrawPacket
{
	FPipeline Pipeline;
	FBuffer Vertices;
	FBuffer Indices;
	std::uint32_t VertexStride{};
	std::uint32_t IndexCount{};
	std::uint32_t FirstIndex{};
	std::int32_t VertexOffset{};
	FRect Scissor;
	FResourceBindingSet Bindings;
	std::vector<FConstantBinding> ConstantBindings;
	FGraphicsDynamicState DynamicState;
	std::uint32_t InstanceCount = 1;
};
enum class EResourceState
{
	Present,
	RenderTarget,
	DepthWrite,
	ShaderRead
};

enum class ERenderTargetKind
{
	None,
	Backbuffer,
	FrameDepth,
	Texture
};

struct FRenderTarget
{
	ERenderTargetKind Kind = ERenderTargetKind::None;
	FTexture Texture;

	static FRenderTarget Backbuffer()
	{
		return {ERenderTargetKind::Backbuffer, {}};
	}

	static FRenderTarget FrameDepth()
	{
		return {ERenderTargetKind::FrameDepth, {}};
	}

	static FRenderTarget FromTexture(FTexture InTexture)
	{
		return {ERenderTargetKind::Texture, std::move(InTexture)};
	}

	bool operator==(const FRenderTarget&) const = default;
};

enum class EAttachmentLoad
{
	Load,
	Clear,
	Discard
};

enum class EAttachmentStore
{
	Store,
	Discard
};

struct FAttachmentActions
{
	EAttachmentLoad Load = EAttachmentLoad::Load;
	EAttachmentStore Store = EAttachmentStore::Store;
	bool operator==(const FAttachmentActions&) const = default;
};

struct FColorAttachment
{
	FRenderTarget Target;
	FAttachmentActions Actions;
	FVec4 Clear;
	bool bSrgb{};
	ERHIColorFormat Format = ERHIColorFormat::Rgba8Unorm;

	ERHIColorFormat GetFormat() const
	{
		return bSrgb ? ERHIColorFormat::Rgba8Srgb : Format;
	}
};

struct FDepthStencilAttachment
{
	FRenderTarget Target;
	ERHIDepthFormat Format = ERHIDepthFormat::None;
	std::optional<FAttachmentActions> Depth;
	std::optional<FAttachmentActions> Stencil;
	float ClearDepth = 1;
	std::uint8_t ClearStencil{};
};

struct FResourceTransition
{
	FRenderTarget Target;
	EResourceState Before = EResourceState::ShaderRead;
	EResourceState After = EResourceState::DepthWrite;
};

// Immutable publication: authors relinquish mutable aliases before sharing.
struct FDrawCommands
{
	std::vector<FDrawPacket> Draws;
	std::shared_ptr<const std::vector<FDrawPacket>> SharedDraws;

	std::span<const FDrawPacket> GetDraws() const
	{
		if (SharedDraws && !Draws.empty())
		{
			throw std::invalid_argument("Pass cannot mix owned and shared draw storage");
		}
		return SharedDraws ? std::span<const FDrawPacket>(*SharedDraws) : std::span<const FDrawPacket>(Draws);
	}

	void ShareDraws()
	{
		(void)GetDraws();
		if (!SharedDraws)
		{
			SharedDraws = std::make_shared<const std::vector<FDrawPacket>>(std::move(Draws));
		}
	}

	void MaterializeDraws()
	{
		if (SharedDraws)
		{
			const auto Source = GetDraws();
			Draws.assign(Source.begin(), Source.end());
			SharedDraws.reset();
		}
	}
};

struct FPassCommands : FDrawCommands
{
	std::string Name;
	std::optional<FColorAttachment> Color;
	std::optional<FDepthStencilAttachment> DepthStencil;
	std::optional<FViewport> Viewport;
	std::vector<FTexture> SampledTextures;
	std::vector<FResourceTransition> Transitions;
	// Color is the legacy single-target spelling; it cannot be combined with Colors.
	std::vector<FColorAttachment> Colors;

	std::span<const FColorAttachment> GetColors() const
	{
		if (Color && !Colors.empty())
		{
			throw std::invalid_argument("Pass cannot mix single and multiple color attachment storage");
		}
		return Color ? std::span<const FColorAttachment>(&*Color, 1) : std::span<const FColorAttachment>(Colors);
	}

	FGraphicsTarget GetGraphicsTarget() const
	{
		const auto Attachments = GetColors();
		if (Attachments.size() > MaximumColorTargets)
		{
			throw std::invalid_argument("Too many color attachments");
		}
		FGraphicsTarget Result{IsSrgb(), GetDepthFormat(), static_cast<std::uint32_t>(Attachments.size())};
		for (std::size_t Index = 0; Index < Attachments.size(); ++Index)
		{
			Result.ColorFormats[Index] = Index == 0 ? Attachments[Index].Format : Attachments[Index].GetFormat();
		}
		return Result;
	}

	bool HasColor() const
	{
		return !GetColors().empty();
	}

	bool HasDepth() const
	{
		return DepthStencil && DepthStencil->Depth.has_value();
	}

	bool HasStencil() const
	{
		return DepthStencil && DepthStencil->Stencil.has_value();
	}

	bool IsSrgb() const
	{
		const auto Attachments = GetColors();
		return !Attachments.empty() && Attachments.front().bSrgb;
	}

	ERHIDepthFormat GetDepthFormat() const
	{
		return DepthStencil ? DepthStencil->Format : ERHIDepthFormat::None;
	}

	FTexture GetDepthTexture() const
	{
		return DepthStencil ? DepthStencil->Target.Texture : FTexture{};
	}
};

struct FGpuPassTiming
{
	std::string Name;
	double Milliseconds{};
};

struct FGpuFrameTiming
{
	std::uint64_t Frame{};
	std::vector<FGpuPassTiming> Passes;
	std::shared_ptr<const void> Swapchain;
};

struct FGpuTimingCapture
{
	std::vector<FGpuFrameTiming> Frames;
	std::uint64_t DroppedFrames{};
};

struct FDeviceStats
{
	std::string Adapter;
	bool bDebugLayer{};
	std::uint64_t ValidationErrors{};
	std::uint64_t SubmittedFrames{};
	std::uint64_t GpuAllocationBytes{};
	std::uint64_t DescriptorAllocations{};
	std::uint64_t DescriptorCopies{};
	std::uint64_t BindingSetsCreated{};
	std::uint64_t GraphicsRootBinds{};
	std::uint64_t GraphicsHeapBinds{};
	std::uint64_t GraphicsConstantBinds{};
	std::uint64_t GraphicsTableBinds{};
	std::uint64_t PipelinesCreated{};
	std::uint64_t ConstantBytesWritten{};
	FGpuFrameTiming GpuTiming; // Last fence-complete sample; may lag the CPU frame.
	std::uint64_t GraphicsPipelineBinds{};
	std::uint64_t GraphicsGeometryBinds{};
	std::uint64_t GraphicsDynamicBinds{};
	std::uint64_t CommandListsCreated{};
	std::uint64_t CommandListResets{};
};

} // namespace Hyperion
