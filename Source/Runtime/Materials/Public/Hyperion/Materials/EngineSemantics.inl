// Engine shader parameters. Field order defines the GPU layout.
HYP_SHADER_CONTRACT(Engine, 4)

HYP_UNIFORM_FLAT_BEGIN(FrameInfo)
	HYP_UNIFORM_WIRE_NAMESPACE("Engine.Frame")
	HYP_UNIFORM_FIELD(float, Time, Frame)
	HYP_SEMANTIC_CONVENTION("Session elapsed seconds")
	HYP_UNIFORM_FIELD(uint, Index, Frame)
	HYP_SEMANTIC_CONVENTION("Session frame serial modulo uint32")
HYP_UNIFORM_END()

HYP_UNIFORM_FLAT_BEGIN(HyperionViewV1)
	HYP_UNIFORM_WIRE_NAMESPACE("Engine.View")
	HYP_UNIFORM_FIELD(float4x4, ViewProjection, View)
	HYP_SEMANTIC_CONVENTION("Column-vector world to clip")
	HYP_SEMANTIC_POLICY(General, true, false, Default)
	HYP_UNIFORM_FIELD(float3, CameraPosition, View)
	HYP_SEMANTIC_CONVENTION("World-space eye position")
	HYP_SEMANTIC_POLICY(General, true, false, Default)
HYP_UNIFORM_END()

HYP_SHADER_VALUE(float4x4, InverseViewProjection, View)
HYP_SEMANTIC_WIRE("Engine.View.InverseViewProjection")
HYP_SHADER_VALUE(float4, Viewport, View)
HYP_SEMANTIC_WIRE("Engine.View.Viewport")
HYP_SHADER_VALUE(float2, DepthRange, View)
HYP_SEMANTIC_WIRE("Engine.View.DepthRange")

HYP_UNIFORM_ALIAS(ViewInfo, HyperionViewV1)
