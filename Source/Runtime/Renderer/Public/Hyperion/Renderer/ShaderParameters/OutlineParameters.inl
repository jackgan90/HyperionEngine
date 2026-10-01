// Outline shader parameters. Field order defines the GPU layout.
HYP_SHADER_CONTRACT(Outline, 1)

HYP_UNIFORM_BEGIN(OutlineV1, Outline)
	HYP_UNIFORM_FIELD(float4, Parameters, Pass)
HYP_UNIFORM_END()

HYP_UNIFORM_BEGIN(OutlineColorV1, OutlineColor)
	HYP_UNIFORM_FIELD(float4, Color, Pass)
HYP_UNIFORM_END()

HYP_RESOURCE_NAMESPACE("Engine.Pass")
HYP_TEXTURE_2D(float, OutlineObjectMask, Pass)
HYP_RESOURCE_SHADER_NAME("ObjectMask")
HYP_TEXTURE_2D(float, OutlineMask, Pass)
