// Sky shader parameters. Field order defines the GPU layout.
HYP_SHADER_CONTRACT(Sky, 1)

HYP_UNIFORM_BEGIN(SkyViewV1, Sky)
	HYP_UNIFORM_FIELD(float4x4, InverseSkyViewProjection, View)
	HYP_UNIFORM_FIELD(float4, SkyViewport, View)
	HYP_UNIFORM_FIELD(float4, SkyRotationIntensity, View)
	HYP_UNIFORM_FIELD(float4, SkyTint, View)
HYP_UNIFORM_END()

HYP_RESOURCE_NAMESPACE("Engine.Pass")
HYP_TEXTURE_CUBE(float4, SkyRadiance, Pass)
HYP_SAMPLER(SkySampler, Pass)
