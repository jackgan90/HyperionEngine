// Output shader parameters. Field order defines the GPU layout.
HYP_SHADER_CONTRACT(Output, 1)

HYP_UNIFORM_BEGIN(OutputV1, Output)
	HYP_UNIFORM_FIELD(float, Exposure, Pass)
HYP_UNIFORM_END()

HYP_RESOURCE_NAMESPACE("Engine.Pass")
HYP_TEXTURE_2D(float4, SceneColor, Pass)
