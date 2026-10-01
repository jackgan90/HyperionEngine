// HierarchicalDepth shader parameters. Field order defines the GPU layout.
HYP_SHADER_CONTRACT(HierarchicalDepth, 1)

HYP_UNIFORM_BEGIN(HZBCopyV1, HierarchicalCopy)
	HYP_UNIFORM_FIELD(uint, Width, Pass)
	HYP_UNIFORM_FIELD(uint, Height, Pass)
	HYP_UNIFORM_FIELD(float2, DepthRange, View)
	HYP_UNIFORM_FIELD(float4, Viewport, View)
	HYP_UNIFORM_FIELD(float, FarDepth, Pass)
HYP_UNIFORM_END()

HYP_UNIFORM_BEGIN(HZBReduceV1, HierarchicalReduce)
	HYP_UNIFORM_FIELD(uint, Width, Pass)
	HYP_UNIFORM_FIELD(uint, Height, Pass)
	HYP_UNIFORM_FIELD(uint, SourceWidth, Pass)
	HYP_UNIFORM_FIELD(uint, SourceHeight, Pass)
	HYP_UNIFORM_FIELD(bool, bMaximum, Pass)
HYP_UNIFORM_END()

HYP_RESOURCE_NAMESPACE("Engine.Pass")
HYP_TEXTURE_2D(float, DepthSource, Pass)
HYP_RESOURCE_SHADER_NAME("SourceDepth")
HYP_RW_TEXTURE_2D(float, DepthOutput, Pass)
HYP_RESOURCE_SHADER_NAME("OutputDepth")
