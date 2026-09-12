#include "SceneInternal.h"
#include <stdexcept>

namespace Hyperion
{
namespace
{
bool EqualOverride(const FMaterialOverride& InA, const FMaterialOverride& InB)
{
	if (InA.BaseColor.has_value() != InB.BaseColor.has_value())
	{
		return false;
	}
	if (InA.BaseColor && (InA.BaseColor->X != InB.BaseColor->X || InA.BaseColor->Y != InB.BaseColor->Y ||
	                      InA.BaseColor->Z != InB.BaseColor->Z || InA.BaseColor->W != InB.BaseColor->W))
	{
		return false;
	}
	return InA.Metallic == InB.Metallic && InA.Roughness == InB.Roughness;
}
} // namespace

bool FSceneModelComponent::operator==(const FSceneModelComponent& InOther) const
{
	return Asset == InOther.Asset && Data == InOther.Data && bVisible == InOther.bVisible &&
	       EqualOverride(Material, InOther.Material) && Surface == InOther.Surface &&
	       SectionSurfaces == InOther.SectionSurfaces;
}

bool FSceneNode::operator==(const FSceneNode& InOther) const
{
	return Id == InOther.Id && Name == InOther.Name && Parent == InOther.Parent &&
	       Local.Values == InOther.Local.Values && bEnabled == InOther.bEnabled && Model == InOther.Model &&
	       Camera == InOther.Camera && DirectionalLight == InOther.DirectionalLight &&
	       EnvironmentLight == InOther.EnvironmentLight;
}

ESceneNodeKind FSceneNode::GetKind() const
{
	if (Model)
	{
		return ESceneNodeKind::Model;
	}
	if (Camera)
	{
		return ESceneNodeKind::Camera;
	}
	if (DirectionalLight)
	{
		return ESceneNodeKind::DirectionalLight;
	}
	if (EnvironmentLight)
	{
		return ESceneNodeKind::EnvironmentLight;
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
	if (!InNode.Model)
	{
		throw std::invalid_argument("Scene node is not a model");
	}
	const auto& Component = *InNode.Model;
	return {InNode.Name,
	        Component.Data,
	        InWorld,
	        bInEffectiveEnabled && Component.bVisible,
	        Component.Material,
	        Component.Surface,
	        Component.SectionSurfaces};
}

FSceneModelComponent SceneModelComponent(const FSceneModel& InModel)
{
	return {{}, InModel.Data, InModel.bVisible, InModel.Material, InModel.Surface, InModel.SectionSurfaces};
}

void ValidateSceneNode(const FSceneNode& InNode)
{
	const auto Payloads = unsigned(InNode.Model.has_value()) + unsigned(InNode.Camera.has_value()) +
	                      unsigned(InNode.DirectionalLight.has_value()) + unsigned(InNode.EnvironmentLight.has_value());
	if (InNode.Id.empty() || Payloads > 1 || !IsAffine(InNode.Local))
	{
		throw std::invalid_argument("Invalid scene node ID, transform or mutually exclusive payload");
	}
	if (InNode.Camera)
	{
		ValidateSceneCamera(*InNode.Camera);
	}
	if (InNode.DirectionalLight)
	{
		ValidateSceneDirectionalLight(*InNode.DirectionalLight);
	}
	if (InNode.EnvironmentLight)
	{
		ValidateSceneEnvironmentLight(*InNode.EnvironmentLight);
	}
	if (InNode.Model)
	{
		ValidateMaterialOverride(InNode.Model->Material);
		ValidateSceneMaterialSelections(SceneModelTransfer(InNode, InNode.Local, true));
	}
}

void ValidateSceneWorld(const FSceneNode& InNode, const FMat4& InWorld)
{
	if (!IsAffine(InWorld))
	{
		throw std::invalid_argument("Scene hierarchy produces a nonfinite or nonaffine world transform");
	}
	if (InNode.Camera || InNode.DirectionalLight)
	{
		ExtractScenePose(InWorld);
	}
}
} // namespace Hyperion
