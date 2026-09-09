#include "D3D12GraphicsState.h"
#include "Hyperion/RHI/RHIPipeline.h"

namespace Hyperion
{
namespace
{
D3D12_FILL_MODE Native(ERHIFill InValue)
{
	switch (InValue)
	{
		case ERHIFill::Solid:
			return D3D12_FILL_MODE_SOLID;
		case ERHIFill::Wireframe:
			return D3D12_FILL_MODE_WIREFRAME;
	}
	throw std::invalid_argument("Unsupported graphics Fill");
}

D3D12_CULL_MODE Native(ERHICull InValue)
{
	switch (InValue)
	{
		case ERHICull::None:
			return D3D12_CULL_MODE_NONE;
		case ERHICull::Front:
			return D3D12_CULL_MODE_FRONT;
		case ERHICull::Back:
			return D3D12_CULL_MODE_BACK;
	}
	throw std::invalid_argument("Unsupported graphics Cull");
}

D3D12_COMPARISON_FUNC Native(ERHICompare InValue)
{
	switch (InValue)
	{
		case ERHICompare::Never:
			return D3D12_COMPARISON_FUNC_NEVER;
		case ERHICompare::Less:
			return D3D12_COMPARISON_FUNC_LESS;
		case ERHICompare::Equal:
			return D3D12_COMPARISON_FUNC_EQUAL;
		case ERHICompare::LessEqual:
			return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case ERHICompare::Greater:
			return D3D12_COMPARISON_FUNC_GREATER;
		case ERHICompare::NotEqual:
			return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case ERHICompare::GreaterEqual:
			return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case ERHICompare::Always:
			return D3D12_COMPARISON_FUNC_ALWAYS;
	}
	throw std::invalid_argument("Unsupported graphics Compare");
}

D3D12_STENCIL_OP Native(ERHIStencilOp InValue)
{
	switch (InValue)
	{
		case ERHIStencilOp::Keep:
			return D3D12_STENCIL_OP_KEEP;
		case ERHIStencilOp::Zero:
			return D3D12_STENCIL_OP_ZERO;
		case ERHIStencilOp::Replace:
			return D3D12_STENCIL_OP_REPLACE;
		case ERHIStencilOp::IncrementClamp:
			return D3D12_STENCIL_OP_INCR_SAT;
		case ERHIStencilOp::DecrementClamp:
			return D3D12_STENCIL_OP_DECR_SAT;
		case ERHIStencilOp::Invert:
			return D3D12_STENCIL_OP_INVERT;
		case ERHIStencilOp::IncrementWrap:
			return D3D12_STENCIL_OP_INCR;
		case ERHIStencilOp::DecrementWrap:
			return D3D12_STENCIL_OP_DECR;
	}
	throw std::invalid_argument("Unsupported graphics StencilOp");
}

D3D12_BLEND Native(ERHIBlendFactor InValue)
{
	switch (InValue)
	{
		case ERHIBlendFactor::Zero:
			return D3D12_BLEND_ZERO;
		case ERHIBlendFactor::One:
			return D3D12_BLEND_ONE;
		case ERHIBlendFactor::SourceColor:
			return D3D12_BLEND_SRC_COLOR;
		case ERHIBlendFactor::InverseSourceColor:
			return D3D12_BLEND_INV_SRC_COLOR;
		case ERHIBlendFactor::SourceAlpha:
			return D3D12_BLEND_SRC_ALPHA;
		case ERHIBlendFactor::InverseSourceAlpha:
			return D3D12_BLEND_INV_SRC_ALPHA;
		case ERHIBlendFactor::DestinationAlpha:
			return D3D12_BLEND_DEST_ALPHA;
		case ERHIBlendFactor::InverseDestinationAlpha:
			return D3D12_BLEND_INV_DEST_ALPHA;
		case ERHIBlendFactor::DestinationColor:
			return D3D12_BLEND_DEST_COLOR;
		case ERHIBlendFactor::InverseDestinationColor:
			return D3D12_BLEND_INV_DEST_COLOR;
		case ERHIBlendFactor::SourceAlphaSaturate:
			return D3D12_BLEND_SRC_ALPHA_SAT;
		case ERHIBlendFactor::Constant:
			return D3D12_BLEND_BLEND_FACTOR;
		case ERHIBlendFactor::InverseConstant:
			return D3D12_BLEND_INV_BLEND_FACTOR;
	}
	throw std::invalid_argument("Unsupported graphics BlendFactor");
}

D3D12_BLEND_OP Native(ERHIBlendOp InValue)
{
	switch (InValue)
	{
		case ERHIBlendOp::Add:
			return D3D12_BLEND_OP_ADD;
		case ERHIBlendOp::Subtract:
			return D3D12_BLEND_OP_SUBTRACT;
		case ERHIBlendOp::ReverseSubtract:
			return D3D12_BLEND_OP_REV_SUBTRACT;
		case ERHIBlendOp::Minimum:
			return D3D12_BLEND_OP_MIN;
		case ERHIBlendOp::Maximum:
			return D3D12_BLEND_OP_MAX;
	}
	throw std::invalid_argument("Unsupported graphics BlendOp");
}

D3D12_DEPTH_STENCILOP_DESC Native(const FStencilFaceDesc& InFace)
{
	return {Native(InFace.Fail), Native(InFace.DepthFail), Native(InFace.Pass), Native(InFace.Compare)};
}
} // namespace

DXGI_FORMAT NativeDepthFormat(ERHIDepthFormat InFormat)
{
	switch (InFormat)
	{
		case ERHIDepthFormat::None:
			return DXGI_FORMAT_UNKNOWN;
		case ERHIDepthFormat::D32:
			return DXGI_FORMAT_D32_FLOAT;
		case ERHIDepthFormat::D32S8:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	}
	throw std::invalid_argument("Unsupported depth format");
}

D3D_PRIMITIVE_TOPOLOGY NativeTopology(ERHIPrimitiveTopology InTopology)
{
	switch (InTopology)
	{
		case ERHIPrimitiveTopology::TriangleList:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case ERHIPrimitiveTopology::LineList:
			return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case ERHIPrimitiveTopology::PointList:
			return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	}
	throw std::invalid_argument("Unsupported geometry topology");
}

void ApplyGraphicsState(D3D12_GRAPHICS_PIPELINE_STATE_DESC& OutPso, const FPipelineDesc& InDesc)
{
	ValidateGraphicsPipeline(InDesc);
	const auto& State = InDesc.State;
	OutPso.SampleMask = State.SampleMask;
	switch (InDesc.Topology)
	{
		case ERHIPrimitiveTopology::TriangleList:
			OutPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			break;
		case ERHIPrimitiveTopology::LineList:
			OutPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			break;
		case ERHIPrimitiveTopology::PointList:
			OutPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
			break;
	}
	OutPso.NumRenderTargets = InDesc.Target.ColorCount;
	OutPso.RTVFormats[0] = InDesc.Target.ColorCount == 0 ? DXGI_FORMAT_UNKNOWN
	                       : InDesc.Target.bSrgb         ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
	                                                     : DXGI_FORMAT_R8G8B8A8_UNORM;
	OutPso.DSVFormat = NativeDepthFormat(InDesc.Target.Depth);
	OutPso.SampleDesc.Count = 1;
	auto& Raster = OutPso.RasterizerState;
	Raster.FillMode = Native(State.Fill);
	Raster.CullMode = Native(State.Cull);
	Raster.FrontCounterClockwise = State.bFrontCounterClockwise;
	Raster.DepthBias = State.DepthBias;
	Raster.DepthBiasClamp = State.DepthBiasClamp;
	Raster.SlopeScaledDepthBias = State.SlopeScaledDepthBias;
	Raster.DepthClipEnable = State.bDepthClip;
	auto& Depth = OutPso.DepthStencilState;
	Depth.DepthEnable = State.bDepthTest;
	Depth.DepthWriteMask = State.bDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	Depth.DepthFunc = Native(State.DepthCompare);
	Depth.StencilEnable = State.bStencil;
	Depth.StencilReadMask = State.StencilReadMask;
	Depth.StencilWriteMask = State.StencilWriteMask;
	Depth.FrontFace = Native(State.FrontStencil);
	Depth.BackFace = Native(State.BackStencil);
	auto& Blend = OutPso.BlendState.RenderTarget[0];
	Blend.RenderTargetWriteMask = State.ColorWriteMask;
	Blend.BlendEnable = State.bBlend;
	Blend.SrcBlend = Native(State.SourceRgb);
	Blend.DestBlend = Native(State.DestinationRgb);
	Blend.BlendOp = Native(State.RgbOperation);
	Blend.SrcBlendAlpha = Native(State.SourceAlpha);
	Blend.DestBlendAlpha = Native(State.DestinationAlpha);
	Blend.BlendOpAlpha = Native(State.AlphaOperation);
}

DXGI_FORMAT NativeVertexFormat(EVertexFormat InFormat)
{
	switch (InFormat)
	{
		case EVertexFormat::Float:
			return DXGI_FORMAT_R32_FLOAT;
		case EVertexFormat::Float2:
			return DXGI_FORMAT_R32G32_FLOAT;
		case EVertexFormat::Float3:
			return DXGI_FORMAT_R32G32B32_FLOAT;
		case EVertexFormat::Float4:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case EVertexFormat::Unorm8x4:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case EVertexFormat::Int:
			return DXGI_FORMAT_R32_SINT;
		case EVertexFormat::Int2:
			return DXGI_FORMAT_R32G32_SINT;
		case EVertexFormat::Int3:
			return DXGI_FORMAT_R32G32B32_SINT;
		case EVertexFormat::Int4:
			return DXGI_FORMAT_R32G32B32A32_SINT;
		case EVertexFormat::Uint:
			return DXGI_FORMAT_R32_UINT;
		case EVertexFormat::Uint2:
			return DXGI_FORMAT_R32G32_UINT;
		case EVertexFormat::Uint3:
			return DXGI_FORMAT_R32G32B32_UINT;
		case EVertexFormat::Uint4:
			return DXGI_FORMAT_R32G32B32A32_UINT;
	}
	throw std::invalid_argument("Unsupported vertex format");
}
} // namespace Hyperion
