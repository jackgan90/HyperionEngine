#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
const FSceneComponent* FindComponent(const FSceneNode& InNode, const FSceneComponentDescriptor& InType)
{
	const FSceneComponent* Result{};
	for (const auto& Component : InNode.Components.All())
	{
		if (Component.Type->Id == InType.Id && Component.Get())
		{
			if (Result)
			{
				return nullptr;
			}
			Result = &Component;
		}
	}
	return Result;
}
} // namespace

void FEditorPlugin::DrawSharedComponent(std::span<const FSceneHandle> InTargets,
                                        const FSceneComponentDescriptor& InType, std::uint64_t InRevision)
{
	std::vector<const void*> Values;
	std::vector<std::string> Ids;
	bool bSameModelAsset = true;
	const auto* Primary = Scene->FindNode(InTargets.front());
	for (const auto Handle : InTargets)
	{
		const auto* Node = Scene->FindNode(Handle);
		const auto* Component = Node ? FindComponent(*Node, InType) : nullptr;
		if (!Component)
		{
			return;
		}
		Values.push_back(Component->Get());
		Ids.push_back(Component->Id);
		if (Primary->Model() && Node->Model())
		{
			bSameModelAsset &=
			    Primary->Model()->Asset == Node->Model()->Asset && Primary->Model()->Data == Node->Model()->Data;
		}
	}
	const auto Identity = "selection/" + Primary->Id + "/" + InType.Id;
	if (!Gui->Section((InType.Label + "##" + Identity).c_str()))
	{
		return;
	}
	FRecordSelectionDraft Draft(*InType.Record, Values);
	const std::vector<std::string> Blocked = InType.CppType == typeid(FSceneModelComponent) && !bSameModelAsset
	                                             ? std::vector<std::string>{"sections"}
	                                             : std::vector<std::string>{};
	Gui->BeginLiveEdit();
	const bool bChanged = Gui->EditRecord(
	    Draft, Identity,
	    [&](std::string_view InField, FVec4 InBounds)
	    {
		    InspectionBounds[InType.Id + "/" + std::string(InField)] = InBounds;
	    },
	    Blocked);
	const auto Edit = Gui->EndLiveEdit();
	if (Edit.ActiveInteraction)
	{
		InspectorInteraction = Edit.ActiveInteraction;
	}
	if (bChanged)
	{
		FPendingInspectorEdit Pending;
		Pending.Revision = InRevision;
		Pending.Interaction = Edit.ChangedInteraction;
		for (std::size_t Index = 0; Index < InTargets.size(); ++Index)
		{
			auto Candidate = *Scene->FindNode(InTargets[Index]);
			Draft.ApplyToCandidate(Index, Candidate.Components.Find(Ids[Index])->Edit());
			Pending.Edits.push_back({InTargets[Index], std::move(Candidate)});
		}
		PendingInspectorEdit = std::move(Pending);
	}
}

void FEditorPlugin::DrawSelectionInspector()
{
	if (Options.bExerciseMultiSelection)
	{
		InspectionBounds.clear();
	}
	const std::vector<FSceneHandle> Targets(Selection.All().rbegin(), Selection.All().rend());
	const auto* Primary = Scene->FindNode(Targets.front());
	if (!Primary)
	{
		return;
	}
	const auto Revision = Scene->GetRevision();
	Gui->TextWrapped(std::to_string(Targets.size()) + " selected | Primary: " + Primary->Name);
	Gui->TextWrapped("Transform fields assign local values to every object. The gizmo transforms the group.");
	bool bMixedEnabled{};
	for (const auto Handle : Targets)
	{
		const auto* Node = Scene->FindNode(Handle);
		if (!Node)
		{
			return;
		}
		bMixedEnabled |= Primary->bEnabled != Node->bEnabled;
	}
	auto Enabled = WriteValue(Primary->bEnabled);
	Gui->BeginLiveEdit();
	Gui->BeginPropertyRow("Enabled");
	const bool bEnabledChanged = Gui->EditMixedScalar(Enabled, {.Kind = ERecordValueKind::Boolean}, {}, bMixedEnabled);
	Gui->EndPropertyRow();
	const auto Edit = Gui->EndLiveEdit();
	if (Edit.ActiveInteraction)
	{
		InspectorInteraction = Edit.ActiveInteraction;
	}
	if (bEnabledChanged)
	{
		FPendingInspectorEdit Pending;
		Pending.Revision = Revision;
		Pending.Interaction = Edit.ChangedInteraction;
		for (const auto Handle : Targets)
		{
			auto Candidate = *Scene->FindNode(Handle);
			Candidate.bEnabled = ReadValue<bool>(Enabled);
			Pending.Edits.push_back({Handle, std::move(Candidate)});
		}
		PendingInspectorEdit = std::move(Pending);
	}
	for (const auto& Component : Primary->Components.All())
	{
		if (Component.Get())
		{
			DrawSharedComponent(Targets, *Component.Type, Revision);
		}
	}
}
} // namespace Hyperion
