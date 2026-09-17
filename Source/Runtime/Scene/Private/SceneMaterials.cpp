#include "Hyperion/Scene/Scene.h"
#include <set>
#include <stdexcept>

namespace Hyperion
{
namespace
{
void ValidateSelection(const FSceneMaterialSelection& InSelection)
{
	if (InSelection.Instance && InSelection.Snapshot)
	{
		throw std::invalid_argument("A material selection cannot contain both an instance and snapshot");
	}
	if (InSelection.Snapshot && (!InSelection.Snapshot->Definition || !InSelection.Snapshot->Schema))
	{
		throw std::invalid_argument("Incomplete scene material snapshot");
	}
	std::set<std::string> Names;
	for (const auto& Override : InSelection.Overrides)
	{
		Override.Value.Validate();
		if (Override.Name.empty() || !Names.insert(Override.Name).second)
		{
			throw std::invalid_argument("Duplicate or empty scene material override");
		}
	}
}
} // namespace

void ValidateSceneMaterialSelections(const FSceneModel& InModel)
{
	std::set<std::string> PrimitiveIds;
	if (InModel.Data && (!InModel.Sections.empty() || !InModel.SourcePrimitive.empty()))
	{
		const auto Instances = SceneModelInstances(InModel);
		for (const auto& Instance : Instances)
		{
			PrimitiveIds.insert(ModelPrimitiveId(*InModel.Data->Asset, Instance.Primitive));
		}
	}
	std::set<std::string> Sections;
	for (const auto& Section : InModel.Sections)
	{
		if (Section.Primitive.empty() || !Sections.insert(Section.Primitive).second ||
		    (InModel.Data && !PrimitiveIds.contains(Section.Primitive)))
		{
			throw std::invalid_argument("Invalid or duplicate Static Mesh section identity");
		}
		ValidateMaterialOverride(Section.Material);
	}
	ValidateSelection(InModel.Surface);
	for (const auto& [Section, Selection] : InModel.SectionSurfaces)
	{
		if (InModel.Data && Section >= InModel.Data->PrimitiveBounds.size())
		{
			throw std::invalid_argument("Scene material selection references an invalid primitive section");
		}
		ValidateSelection(Selection);
	}
}
} // namespace Hyperion
