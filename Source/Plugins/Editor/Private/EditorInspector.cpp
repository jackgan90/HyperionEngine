#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
bool FEditorPlugin::DrawComponent(const FSceneNodeView& InView, const FSceneComponent& InComponent,
                                  std::uint64_t InRevision)
{
	const auto& Node = *InView.Node;
	const auto Identity = Node.Id + "/" + InComponent.Id;
	const bool bOpen = Gui->Section((InComponent.Type->Label + "##" + Identity).c_str());
	if (!Options.ExerciseDocument.empty())
	{
		InspectionBounds[InComponent.Type->Id + "/header"] = Gui->LastItemBounds();
	}
	if (!bOpen)
	{
		return false;
	}
	auto Draft = InspectorDrafts.find(InComponent.Id);
	if (Draft != InspectorDrafts.end() && !Draft->second.bModified && Draft->second.Revision != InRevision)
	{
		InspectorDrafts.erase(Draft);
		Draft = InspectorDrafts.end();
	}
	if (Draft == InspectorDrafts.end())
	{
		Draft = InspectorDrafts
		            .emplace(InComponent.Id,
		                     FInspectionDraft{FRecordDraft(*InComponent.Type->Record, InComponent.Get()), InRevision})
		            .first;
	}
	auto& State = Draft->second;
	State.bModified |= Gui->EditRecord(State.Record, Identity, Options.ExerciseDocument.empty() ?
	    std::function<void(std::string_view, FVec4)>{} : [&](std::string_view InField, FVec4 InBounds)
	    { InspectionBounds[InComponent.Type->Id + "/" + std::string(InField)] = InBounds; });
	if (State.bModified && State.Revision != InRevision)
	{
		Gui->TextWrapped("This draft is stale. Revert to read the current component.");
	}
	if (Gui->Button(("Apply##" + Identity).c_str(), State.bModified && State.Revision == InRevision))
	{
		auto Candidate = Node;
		State.Record.ApplyToCandidate(Candidate.Components.Find(InComponent.Id)->Edit());
		CommitEdit(InView.Handle, std::move(Candidate), State.Revision);
		InspectorDrafts.erase(Draft);
		return true;
	}
	if (!Options.ExerciseDocument.empty())
	{
		InspectionBounds[InComponent.Type->Id + "/apply"] = Gui->LastItemBounds();
	}
	Gui->SameLine();
	if (Gui->Button(("Revert##" + Identity).c_str(), State.bModified))
	{
		InspectorDrafts.erase(Draft);
		return true;
	}
	if (!InComponent.Type->bRequired)
	{
		Gui->SameLine();
		if (Gui->Button(("Remove##" + Identity).c_str(), !HasDrafts()))
		{
			auto Candidate = Node;
			Candidate.Components.Remove(InComponent.Id);
			CommitEdit(InView.Handle, std::move(Candidate), InRevision);
			return true;
		}
	}
	if (InComponent.Type->CppType == typeid(FSceneModelComponent) &&
	    Gui->Section(("Rendering diagnostics (read only)##" + Identity).c_str(), false))
	{
		const auto Diagnostics = Scene->GetComponentDiagnostics(InView.Handle, InComponent.Id);
		if (Diagnostics && Diagnostics->Object == InView.Handle && Diagnostics->Component == InComponent.Id)
		{
			FRecordDraft Readback(RecordType<FSceneComponentDiagnostics>(), Diagnostics.get());
			Gui->EditRecord(Readback, Identity + "/render");
		}
	}
	return false;
}

void FEditorPlugin::DrawComponentInspector(const FSceneNodeView& InView)
{
	const auto& Node = *InView.Node;
	const auto Revision = Scene->GetRevision();
	if (InspectedObject != InView.Handle)
	{
		InspectorDrafts.clear();
		InspectedObject = InView.Handle;
	}
	auto Name = Node.Name;
	bool bEnabled = Node.bEnabled;
	const bool bNameChanged = Gui->InputText("Object name", Name);
	const bool bEnabledChanged = Gui->Checkbox("Enabled", bEnabled);
	if (bNameChanged || bEnabledChanged)
	{
		auto Candidate = Node;
		Candidate.Name = std::move(Name);
		Candidate.bEnabled = bEnabled;
		CommitEdit(InView.Handle, std::move(Candidate), Revision);
		return;
	}
	Gui->TextWrapped("Object ID: " + Node.Id);
	if (Node.Camera())
	{
		DrawCameraActions(InView.Handle);
		if (Scene->GetRevision() != Revision)
		{
			return;
		}
	}
	for (const auto& Component : Node.Components.All())
	{
		if (!Component.Get())
		{
			continue;
		}
		if (DrawComponent(InView, Component, Revision))
		{
			return;
		}
	}
	for (const auto& [Id, Envelope] : Node.Components.Unknown())
	{
		Gui->TextWrapped("Unavailable component: " + Id + ". Saving is blocked until its type is registered.");
	}
	if (Gui->BeginMenu("Add component"))
	{
		Gui->BeginDisabled(HasDrafts());
		for (const auto& Type : SceneComponentRegistry().All())
		{
			const bool bPresent =
			    Type->bUnique && std::any_of(Node.Components.All().begin(), Node.Components.All().end(),
			                                 [&](const auto& InComponent)
			                                 {
				                                 return InComponent.Type == Type && InComponent.Get();
			                                 });
			if (!bPresent && !Type->bRequired && Type->CppType != typeid(FSceneModelComponent) &&
			    Type->CppType != typeid(FSceneModelSource) && Gui->MenuItem(Type->Label.c_str()))
			{
				Gui->EndDisabled();
				Gui->EndMenu();
				auto Candidate = Node;
				Candidate.Components.Add(Type->Id + "-" + std::to_string(Revision + 1), Type->Id);
				CommitEdit(InView.Handle, std::move(Candidate), Revision);
				return;
			}
		}
		Gui->EndDisabled();
		Gui->EndMenu();
	}
}
} // namespace Hyperion
