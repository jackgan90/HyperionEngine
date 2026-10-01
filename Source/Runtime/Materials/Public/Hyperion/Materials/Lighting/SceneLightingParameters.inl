// SceneLighting shader parameters. Field order defines the GPU layout.
HYP_SHADER_CONTRACT(SceneLighting, 1)

HYP_CONTRACT_POLICY(General, true, true)

HYP_UNIFORM_FLAT_BEGIN(HyperionSceneV1)
	HYP_UNIFORM_WIRE_NAMESPACE("Engine.Scene")
	HYP_UNIFORM_FIELD(float3, MainLightDirection, Scene)
	HYP_SEMANTIC_WIRE("Engine.Scene.MainDirectionalLightDirection")
	HYP_SEMANTIC_CONVENTION("Unit world-space surface-to-light vector")
	HYP_UNIFORM_FIELD(float3, MainLightColor, Scene)
	HYP_SEMANTIC_WIRE("Engine.Scene.MainDirectionalLightColor")
	HYP_SEMANTIC_CONVENTION("Linear RGB radiance")
	HYP_UNIFORM_FIELD(float3, AmbientColor, Scene)
	HYP_SEMANTIC_CONVENTION("Linear ambient RGB radiance")
HYP_UNIFORM_END()

HYP_RESOURCE_NAMESPACE("Engine.Scene")
HYP_READ_BUFFER(FDirectionalLight, DirectionalLights, Scene)
HYP_RESOURCE_SHADER_NAME("HyperionDirectionalLightsV1")
HYP_SEMANTIC_CONVENTION("Unshadowed directional lights; excludes the separately evaluated shadow source")

HYP_STRUCTURED_BEGIN(FDirectionalLight)
	HYP_STRUCTURED_FIELD(float4, Direction)
	HYP_STRUCTURED_FIELD(float4, Radiance)
HYP_STRUCTURED_END()

HYP_UNIFORM_ALIAS(SceneInfo, HyperionSceneV1)
