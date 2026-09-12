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
	std::vector<std::pair<FSceneHandle, FSceneNodeModel>> Selections;
	for (const auto& Entry : Manifest->Nodes)
	{
		if (Entry.Model)
		{
			const auto Handle = Scene.FindHandle(Entry.Id);
			const bool bStored = HasSelection(Entry.Model->Surface) || !Entry.Model->SectionSurfaces.empty();
			PendingMaterials.emplace(Handle, FPendingMaterial{bStored});
			if (bStored)
			{
				Selections.emplace_back(Handle, *Entry.Model);
			}
		}
	}
	bMaterialsComplete = Selections.empty();
	if (bMaterialsComplete)
	{
		return;
	}
	MaterialCancellation = {};
	MaterialPreparation = DispatchAsync<std::vector<FSelectedMaterials>>(
	    Tasks, {EDomain::Worker},
	    [this, Source = std::move(Selections), Epoch = LoadEpoch, Containing = Path,
	     Cancellation = MaterialCancellation]
	    {
		    std::vector<FSelectedMaterials> Result;
		    for (const auto& [Handle, Entry] : Source)
		    {
			    Cancellation.Check();
			    FSelectedMaterials Selection;
			    Selection.Handle = Handle;
			    Selection.Epoch = Epoch;
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
			if (Selection.Epoch == LoadEpoch && Scene.FindModelComponent(Selection.Handle) &&
			    PendingMaterials.contains(Selection.Handle))
			{
				SelectedMaterials.emplace(Selection.Handle, Selection);
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
		const auto* Model = Scene.FindModelComponent(Instance.Handle);
		const auto Load = Loads.find(Instance.Asset);
		if (!Model || Model->Data || Load == Loads.end() || !Load->second.Data || Load->second.Epoch != LoadEpoch)
		{
			continue;
		}
		const auto Selection = SelectedMaterials.find(Instance.Handle);
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
				ApplyLoadedMaterials(Updated, Selection->second, PendingMaterials.at(Instance.Handle));
			}
			Scene.SetModelComponent(Instance.Handle, std::move(Updated));
			PendingMaterials.erase(Instance.Handle);
		}
		catch (const std::exception& Error)
		{
			SelectedMaterials[Instance.Handle].Error = Error.what();
		}
		bStatusDirty = true;
	}
}

FSceneInstance::FImpl::FPendingMaterial FSceneInstance::FImpl::PrepareMaterialEdits(
    const FPendingMaterial& InPending, const FSceneModelComponent& InBefore, const FSceneModelComponent& InAfter) const
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

void FSceneInstance::FImpl::ApplyLoadedMaterials(FSceneModelComponent& InModel, const FSelectedMaterials& InSelection,
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
