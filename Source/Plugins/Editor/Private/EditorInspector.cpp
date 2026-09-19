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
	FRecordDraft Record(*InComponent.Type->Record, InComponent.Get());
	Gui->BeginLiveEdit();
	const bool bChanged = Gui->EditRecord(Record, Identity, Options.ExerciseDocument.empty() ?
	    std::function<void(std::string_view, FVec4)>{} : [&](std::string_view InField, FVec4 InBounds)
	    { InspectionBounds[InComponent.Type->Id + "/" + std::string(InField)] = InBounds; });
	const auto Edit = Gui->EndLiveEdit();
	if (Edit.ActiveInteraction)
	{
		InspectorInteraction = Edit.ActiveInteraction;
	}
	if (bChanged)
	{
		try
		{
			auto Candidate = Node;
			Record.ApplyToCandidate(Candidate.Components.Find(InComponent.Id)->Edit());
			PendingInspectorEdit =
			    FPendingInspectorEdit{InView.Handle, std::move(Candidate), InRevision, Edit.ChangedInteraction};
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
	if (!InComponent.Type->bRequired)
	{
		if (Gui->Button(("Remove##" + Identity).c_str()))
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
	auto Name = Node.Name;
	bool bEnabled = Node.bEnabled;
	Gui->BeginLiveEdit();
	Gui->BeginPropertyRow("Object name");
	const bool bNameChanged = Gui->InputText("##value", Name);
	Gui->EndPropertyRow();
	Gui->BeginPropertyRow("Enabled");
	const bool bEnabledChanged = Gui->Checkbox("##value", bEnabled);
	Gui->EndPropertyRow();
	const auto Edit = Gui->EndLiveEdit();
	if (Edit.ActiveInteraction)
	{
		InspectorInteraction = Edit.ActiveInteraction;
	}
	if (bNameChanged || bEnabledChanged)
	{
		auto Candidate = Node;
		Candidate.Name = std::move(Name);
		Candidate.bEnabled = bEnabled;
		PendingInspectorEdit =
		    FPendingInspectorEdit{InView.Handle, std::move(Candidate), Revision, Edit.ChangedInteraction};
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
				Gui->EndMenu();
				auto Candidate = Node;
				Candidate.Components.Add(Type->Id + "-" + std::to_string(Revision + 1), Type->Id);
				CommitEdit(InView.Handle, std::move(Candidate), Revision);
				return;
			}
		}
		Gui->EndMenu();
	}
}
} // namespace Hyperion
