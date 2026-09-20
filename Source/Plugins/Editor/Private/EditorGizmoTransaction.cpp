#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::BeginGizmoEdit(const FSceneNodeView& InView, FVec4 InBounds)
{
	FGizmoEdit Edit{*Selection, InView.Node->Local(), InView.Node->Local(), Scene->GetRevision(), InBounds};
	Edit.bGroup = Selection.All().size() > 1;
	if (Edit.bGroup)
	{
		(void)Gizmo.GroupDelta(Edit.Initial);
	}
	for (const auto Handle : SelectedRoots())
	{
		FSceneNodeView View;
		if (!Scene->GetNodeView(Handle, View))
		{
			throw std::runtime_error("Group transform target is unavailable");
		}
		FMat4 ParentInverse = Identity();
		if (Edit.bGroup && !View.Node->Parent().empty())
		{
			FSceneNodeView Parent;
			if (!Scene->GetNodeView(Scene->FindHandle(View.Node->Parent()), Parent))
			{
				throw std::runtime_error("Group transform parent is unavailable");
			}
			ParentInverse = Inverse(Parent.World);
			if (!IsAffine(ParentInverse))
			{
				throw std::runtime_error("Group transform requires invertible selected-root parents");
			}
		}
		Edit.Targets.push_back({Handle, View.Node->Local(), View.World, ParentInverse, View.Node->Local()});
	}
	GizmoEdit = std::move(Edit);
}

void FEditorPlugin::PreviewGizmoEdit(const FMat4& InPrimaryLocal)
{
	auto& Edit = *GizmoEdit;
	const auto Delta = Edit.bGroup ? Gizmo.GroupDelta(InPrimaryLocal) : Identity();
	std::vector<FSceneNodeEdit> Candidates;
	std::vector<FMat4> Previews;
	bool bChanged{};
	for (const auto& Target : Edit.Targets)
	{
		const auto* Node = Scene->FindNode(Target.Handle);
		if (!Node || Node->Local().Values != Target.Preview.Values)
		{
			throw std::runtime_error("Group transform target changed externally");
		}
		auto Candidate = *Node;
		Candidate.Local() = Target.Handle == Edit.Handle ? InPrimaryLocal
		                    : Delta.Values == Identity().Values
		                        ? Target.Initial
		                        : Multiply(Target.ParentInverse, Multiply(Delta, Target.World));
		bChanged |= Candidate.Local().Values != Target.Preview.Values;
		Previews.push_back(Candidate.Local());
		Candidates.push_back({Target.Handle, std::move(Candidate)});
	}
	if (bChanged && !Scene->EditNodes(std::move(Candidates), Edit.Revision))
	{
		throw std::runtime_error("Group transform revision changed");
	}
	for (std::size_t Index = 0; Index < Edit.Targets.size(); ++Index)
	{
		Edit.Targets[Index].Preview = Previews[Index];
	}
	Edit.Preview = InPrimaryLocal;
	Edit.Revision = Scene->GetRevision();
}

void FEditorPlugin::FinishGizmo(bool bInCancel)
{
	Gizmo.End();
	if (Gui)
	{
		Gui->CaptureImagePointer(false);
	}
	if (!GizmoEdit)
	{
		return;
	}
	const auto Edit = std::exchange(GizmoEdit, {});
	try
	{
		std::vector<FSceneNodeEdit> Before;
		std::vector<FSceneNodeEdit> After;
		bool bConflict{};
		bool bChanged{};
		for (const auto& Target : Edit->Targets)
		{
			const auto* Node = Scene->FindNode(Target.Handle);
			if (!Node || Node->Local().Values != Target.Preview.Values)
			{
				bConflict = true;
				continue;
			}
			bChanged |= Node->Local().Values != Target.Initial.Values;
			After.push_back({Target.Handle, *Node});
			auto Candidate = *Node;
			Candidate.Local() = Target.Initial;
			Before.push_back({Target.Handle, std::move(Candidate)});
		}
		if (!bChanged)
		{
			return;
		}
		// Restore only transforms still owned by this gesture; preserve asynchronous property updates.
		if (!Scene->EditNodes(std::move(Before), Scene->GetRevision()))
		{
			throw std::runtime_error("Transform targets are no longer current");
		}
		if (!bInCancel && !bConflict)
		{
			CommitEdits(std::move(After), Scene->GetRevision());
		}
		if (bConflict)
		{
			Error = "Group manipulation canceled because a target changed externally";
		}
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}
} // namespace Hyperion
