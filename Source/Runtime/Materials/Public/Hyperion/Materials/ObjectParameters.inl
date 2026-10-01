// Object shader parameters. Field order defines the GPU layout.
HYP_SHADER_CONTRACT(Object, 1)

HYP_UNIFORM_FLAT_BEGIN(HyperionObjectV1)
	HYP_UNIFORM_WIRE_NAMESPACE("Engine.Object")
	HYP_UNIFORM_FIELD(float4x4, World, Object)
	HYP_SEMANTIC_CONVENTION("Column-vector local to world")
	HYP_UNIFORM_FIELD(float4x4, Normal, Object)
	HYP_SEMANTIC_CONVENTION("Inverse transpose local to world")
	HYP_UNIFORM_FIELD(float, OrientationSign, Object)
	HYP_SEMANTIC_CONVENTION("World determinant sign")
HYP_UNIFORM_END()

HYP_UNIFORM_FLAT_BEGIN(DrawConstants)
	HYP_UNIFORM_WIRE_NAMESPACE("Engine.Object")
	HYP_UNIFORM_FIELD(float4x4, TransformMatrix, Object)
	HYP_SEMANTIC_WIRE("Engine.Object.WorldViewProjection")
	HYP_SEMANTIC_CONVENTION("Derived from World and ViewProjection")
HYP_UNIFORM_END()

HYP_UNIFORM_ALIAS(ObjectInfo, HyperionObjectV1)
HYP_INSTANCE_ARRAY(HyperionObjectV1, ObjectInstances)
HYP_INSTANCE_ARRAY(DrawConstants, DrawInstances)
