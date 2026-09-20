#include "Hyperion/Scene/SceneNode.h"

namespace Hyperion
{
FSceneComponentRegistry& SceneComponentRegistry()
{
	static FSceneComponentRegistry Registry;
	static const bool bRegistered = []
	{
		Registry.Register(MakeSceneComponent<FSceneTransform>("Transform (Local)", true));
		Registry.Register(MakeSceneComponent<FSceneModelSource>("Model Source"));
		Registry.Register(MakeSceneComponent<FSceneModelComponent>("Model"));
		Registry.Register(MakeSceneComponent<FSceneCamera>("Camera"));
		Registry.Register(MakeSceneComponent<FSceneDirectionalLight>("Directional Light"));
		Registry.Register(MakeSceneComponent<FSceneEnvironmentLight>("Environment Light"));
		Registry.Register(MakeSceneComponent<FScenePointLight>("Point Light"));
		Registry.Register(MakeSceneComponent<FSceneSpotLight>("Spot Light"));
		return true;
	}();
	(void)bRegistered;
	return Registry;
}

FSceneNode::FSceneNode()
{
	Components.Slot<FSceneTransform>().emplace();
}

bool FSceneNode::Has(ESceneNodeKind InKind) const
{
	switch (InKind)
	{
		case ESceneNodeKind::Model:
			return Model().has_value();
		case ESceneNodeKind::Camera:
			return Camera().has_value();
		case ESceneNodeKind::DirectionalLight:
			return DirectionalLight().has_value();
		case ESceneNodeKind::EnvironmentLight:
			return EnvironmentLight().has_value();
		case ESceneNodeKind::PointLight:
			return PointLight().has_value();
		case ESceneNodeKind::SpotLight:
			return SpotLight().has_value();
		case ESceneNodeKind::Group:
			return GetKind() == ESceneNodeKind::Group;
	}
	return false;
}

template<> const FRecordDescriptor& RecordType<FSceneModelComponent>()
{
	static const auto Type = MakeRecord<FSceneModelComponent>(
	    "hyperion.staticmesh",
	    {Member("asset", &FSceneModelComponent::Asset,
	            {.Inspector = FPropertyPresentation{"Model asset", {}, true, {}, {}, {}, "hyperion.modelasset"}}),
	     Member("visible", &FSceneModelComponent::bVisible, {.Inspector = FPropertyPresentation{"Visible"}}),
	     Member("sourceNode", &FSceneModelComponent::SourceNode, Inspect("Source node", {}, {}, true)),
	     Member("sourcePrimitive", &FSceneModelComponent::SourcePrimitive, Inspect("Source primitive", {}, {}, true)),
	     Member("sections", &FSceneModelComponent::Sections,
	            {.Inspector = FPropertyPresentation{.Label = "Mesh sections",
	                                                .bAllowResize = false,
	                                                .ElementIdentity = "primitive"}}),
	     Member("material", &FSceneModelComponent::Material,
	            {.Inspector = FPropertyPresentation{"Material overrides"}})},
	    1,
	    [](const FSceneModelComponent& InValue)
	    {
		    ValidateMaterialOverride(InValue.Material);
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneModelSource>()
{
	static const auto Type = MakeRecord<FSceneModelSource>(
	    "hyperion.modelsourcecomponent",
	    {Member("asset", &FSceneModelSource::Asset, Inspect("Model asset", {}, {}, true)),
	     Member("sourceNode", &FSceneModelSource::SourceNode, Inspect("Source node", {}, {}, true)),
	     Member("instanceRoot", &FSceneModelSource::InstanceRoot, Inspect("Instance root", {}, {}, true))});
	return Type;
}
} // namespace Hyperion
