#include "Hyperion/Renderer/RenderSession.h"
#include "SceneInstanceInternal.h"

namespace Hyperion
{
namespace
{
bool HasSelection(const FSceneMaterialAsset& InMaterial)
{
	return InMaterial.Reference || !InMaterial.Values.empty() || !InMaterial.Overrides.empty();
}
} // namespace

void FSceneInstance::FImpl::BeginMaterials()
{
	for (const auto& Entry : Manifest->Instances)
	{
		PendingMaterials.emplace(Entry.Id,
		                         FPendingMaterial{HasSelection(Entry.Surface) || !Entry.SectionSurfaces.empty()});
	}
	bMaterialsComplete = std::none_of(Manifest->Instances.begin(), Manifest->Instances.end(),
	                                  [](const auto& InEntry)
	                                  {
		                                  return HasSelection(InEntry.Surface) || !InEntry.SectionSurfaces.empty();
	                                  });
	if (bMaterialsComplete)
	{
		return;
	}
	MaterialCancellation = {};
	MaterialPreparation = DispatchAsync<std::vector<FSelectedMaterials>>(
	    Tasks, {EDomain::Worker},
	    [this, Source = Manifest, Containing = Path, Cancellation = MaterialCancellation]
	    {
		    std::vector<FSelectedMaterials> Result;
		    for (const auto& Entry : Source->Instances)
		    {
			    Cancellation.Check();
			    FSelectedMaterials Selection;
			    Selection.Id = Entry.Id;
			    try
			    {
				    if (HasSelection(Entry.Surface))
				    {
					    Selection.Surface = *LoadSceneMaterialSelection(Assets, Tasks, Session.GetResources(),
					                                                    Entry.Surface, Containing, Cancellation)
					                             .Get(Tasks);
				    }
				    for (const auto& Section : Entry.SectionSurfaces)
				    {
					    Selection.Sections.emplace(
					        Section.Section, *LoadSceneMaterialSelection(Assets, Tasks, Session.GetResources(),
					                                                     Section.Material, Containing, Cancellation)
					                              .Get(Tasks));
				    }
			    }
			    catch (const std::exception& Error)
			    {
				    Cancellation.Check();
				    Selection.Error = Error.what();
			    }
			    Result.push_back(std::move(Selection));
		    }
		    return Result;
	    },
	    MaterialCancellation);
}

void FSceneInstance::FImpl::PollMaterials()
{
	if (!bMaterialsComplete && MaterialPreparation.Ready())
	{
		for (const auto& Selection : *MaterialPreparation.GetReady())
		{
			if (PendingMaterials.contains(Selection.Id))
			{
				SelectedMaterials.emplace(Selection.Id, Selection);
			}
		}
		MaterialPreparation = {};
		bMaterialsComplete = true;
		bStatusDirty = true;
	}
}

void FSceneInstance::FImpl::PublishModels()
{
	if (!bMaterialsComplete)
	{
		return;
	}
	for (const auto& Instance : Models)
	{
		const auto* Model = Scene.Find(Instance.Handle);
		const auto Load = Loads.find(Instance.Asset);
		if (!Model || Model->Data || Load == Loads.end() || !Load->second.Data)
		{
			continue;
		}
		const auto Selection = SelectedMaterials.find(Instance.Id);
		if (Selection != SelectedMaterials.end() && !Selection->second.Error.empty())
		{
			continue;
		}
		try
		{
			auto Updated = *Model;
			Updated.Data = Load->second.Data;
			if (Selection != SelectedMaterials.end())
			{
				ApplyLoadedMaterials(Updated, Selection->second, PendingMaterials.at(Instance.Id));
			}
			Scene.Update(Instance.Handle, std::move(Updated));
			PendingMaterials.erase(Instance.Id);
		}
		catch (const std::exception& Error)
		{
			SelectedMaterials[Instance.Id].Error = Error.what();
		}
		bStatusDirty = true;
	}
}

FSceneInstance::FImpl::FPendingMaterial FSceneInstance::FImpl::PrepareMaterialEdits(const FPendingMaterial& InPending,
                                                                                    const FSceneModel& InBefore,
                                                                                    const FSceneModel& InAfter) const
{
	auto Result = InPending;
	Result.bSurfaceEdited |= InBefore.Surface != InAfter.Surface;
	for (const auto* Sections : {&InBefore.SectionSurfaces, &InAfter.SectionSurfaces})
	{
		for (const auto& [Section, Selection] : *Sections)
		{
			const auto Before = InBefore.SectionSurfaces.find(Section);
			const auto After = InAfter.SectionSurfaces.find(Section);
			if (Before == InBefore.SectionSurfaces.end() || After == InAfter.SectionSurfaces.end() ||
			    Before->second != After->second)
			{
				Result.EditedSections.insert(Section);
			}
		}
	}
	return Result;
}

void FSceneInstance::FImpl::ApplyLoadedMaterials(FSceneModel& InModel, const FSelectedMaterials& InSelection,
                                                 const FPendingMaterial& InPending) const
{
	if (!InPending.bSurfaceEdited)
	{
		InModel.Surface = InSelection.Surface;
	}
	auto Sections = InSelection.Sections;
	for (const auto Section : InPending.EditedSections)
	{
		const auto Edited = InModel.SectionSurfaces.find(Section);
		if (Edited == InModel.SectionSurfaces.end())
		{
			Sections.erase(Section);
		}
		else
		{
			Sections[Section] = Edited->second;
		}
	}
	InModel.SectionSurfaces = std::move(Sections);
}
} // namespace Hyperion
