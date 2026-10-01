// DepthPreview shader parameters. Field order defines the GPU layout.
HYP_SHADER_CONTRACT(DepthPreview, 1)

HYP_UNIFORM_BEGIN(PreviewV1, Preview)
	HYP_UNIFORM_FIELD(uint, Mip, Pass)
	HYP_UNIFORM_FIELD(bool, bInvert, Pass)
	HYP_UNIFORM_FIELD(float4, Viewport, View)
HYP_UNIFORM_END()

HYP_RESOURCE_NAMESPACE("Engine.Pass")
HYP_TEXTURE_2D(float4, PreviewSource, Pass)
HYP_RESOURCE_SHADER_NAME("HyperionPreviewSource")
HYP_TEXTURE_2D(float, DepthPreviewMap, Pass)
HYP_RESOURCE_SHADER_NAME("DepthMap")
