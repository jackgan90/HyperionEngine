#include "SceneInternal.h"
#include <stdexcept>

namespace Hyperion
{
bool FSceneModelComponent::operator==(const FSceneModelComponent& InOther) const
{
	return Asset == InOther.Asset && Data == InOther.Data && bVisible == InOther.bVisible &&
	       Material == InOther.Material && Surface == InOther.Surface && SectionSurfaces == InOther.SectionSurfaces &&
	       SourceNode == InOther.SourceNode && Sections == InOther.Sections &&
	       SourcePrimitive == InOther.SourcePrimitive;
}

bool FSceneNode::operator==(const FSceneNode& InOther) const
{
	return Id == InOther.Id && Name == InOther.Name && bEnabled == InOther.bEnabled && Components == InOther.Components;
}

ESceneNodeKind FSceneNode::GetKind() const
{
	if (Model())
	{
		return ESceneNodeKind::Model;
	}
	if (Camera())
	{
		return ESceneNodeKind::Camera;
	}
	if (DirectionalLight())
	{
		return ESceneNodeKind::DirectionalLight;
	}
	if (EnvironmentLight())
	{
		return ESceneNodeKind::EnvironmentLight;
	}
	if (PointLight())
	{
		return ESceneNodeKind::PointLight;
	}
	if (SpotLight())
	{
		return ESceneNodeKind::SpotLight;
	}
	return ESceneNodeKind::Group;
}

const char* ToString(ESceneNodeKind InKind)
{
	switch (InKind)
	{
		case ESceneNodeKind::Group:
			return "Group";
		case ESceneNodeKind::Model:
			return "Model";
		case ESceneNodeKind::Camera:
			return "Camera";
		case ESceneNodeKind::DirectionalLight:
			return "DirectionalLight";
		case ESceneNodeKind::PointLight:
			return "PointLight";
		case ESceneNodeKind::SpotLight:
			return "SpotLight";
		case ESceneNodeKind::EnvironmentLight:
			return "EnvironmentLight";
	}
	return "Unknown";
}

ESceneChangeMask operator|(ESceneChangeMask InA, ESceneChangeMask InB)
{
	return static_cast<ESceneChangeMask>(static_cast<std::uint32_t>(InA) | static_cast<std::uint32_t>(InB));
}

ESceneChangeMask& operator|=(ESceneChangeMask& InA, ESceneChangeMask InB)
{
	InA = InA | InB;
	return InA;
}

bool HasChange(ESceneChangeMask InMask, ESceneChangeMask InFlags)
{
	return (static_cast<std::uint32_t>(InMask) & static_cast<std::uint32_t>(InFlags)) != 0;
}

FSceneModel SceneModelTransfer(const FSceneNode& InNode, const FMat4& InWorld, bool bInEffectiveEnabled)
{
	if (!InNode.Model())
	{
		throw std::invalid_argument("Scene node is not a model");
	}
	const auto& Component = *InNode.Model();
	return {InNode.Name,
	        Component.Data,
	        InWorld,
	        bInEffectiveEnabled && Component.bVisible,
	        Component.Material,
	        Component.Surface,
	        Component.SectionSurfaces,
	        Component.SourceNode,
	        Component.Sections,
	        Component.SourcePrimitive};
}

FSceneModelComponent SceneModelComponent(const FSceneModel& InModel)
{
	return {{},
	        InModel.Data,
	        InModel.bVisible,
	        InModel.Material,
	        InModel.Surface,
	        InModel.SectionSurfaces,
	        InModel.SourceNode,
	        InModel.Sections,
	        InModel.SourcePrimitive};
}

void ValidateSceneNode(const FSceneNode& InNode)
{
	if (InNode.Id.empty())
	{
		throw std::invalid_argument("Scene object requires an ID");
	}
	InNode.Components.Validate();
	if (InNode.Camera())
	{
		ValidateSceneCamera(*InNode.Camera());
	}
	if (InNode.DirectionalLight())
	{
		ValidateSceneDirectionalLight(*InNode.DirectionalLight());
	}
	if (InNode.EnvironmentLight())
	{
		ValidateSceneEnvironmentLight(*InNode.EnvironmentLight());
	}
	if (InNode.PointLight())
	{
		ValidateScenePointLight(*InNode.PointLight());
	}
	if (InNode.SpotLight())
	{
		ValidateSceneSpotLight(*InNode.SpotLight());
	}
	if (InNode.Model())
	{
		if (InNode.Model()->Data && !InNode.Model()->SourceNode.empty())
		{
			const auto& Instances = InNode.Model()->Data->NodeInstances.at(InNode.Model()->SourceNode);
			if (Instances.empty())
			{
				throw std::invalid_argument("Static Mesh source node has no mesh sections");
			}
		}
		ValidateMaterialOverride(InNode.Model()->Material);
		ValidateSceneMaterialSelections(SceneModelTransfer(InNode, InNode.Local(), true));
	}
}

void ValidateSceneWorld(const FSceneNode& InNode, const FMat4& InWorld)
{
	if (!IsAffine(InWorld))
	{
		throw std::invalid_argument("Scene hierarchy produces a nonfinite or nonaffine world transform");
	}
	if (InNode.Camera() || InNode.DirectionalLight() || InNode.SpotLight())
	{
		ExtractScenePose(InWorld);
	}
}
} // namespace Hyperion
