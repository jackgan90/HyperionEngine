#include "Hyperion/SceneEditing/SceneComponentEditing.h"
#include "Support/SceneChangeTestSupport.h"

namespace
{
using namespace Hyperion;
using namespace Hyperion::Tests;
constexpr auto Added = ESceneComponentChangeFlags::Added;
constexpr auto Modified = ESceneComponentChangeFlags::Modified;
constexpr auto Removed = ESceneComponentChangeFlags::Removed;

void CheckComponentDomainFacts(FTaskSystem& InTasks, ISceneEditTarget& InTarget, FScene& InScene)
{
	auto Node = MakeSceneCameraNode("facts-domain");
	AddSceneFactValue(Node, "first", 1);
	AddSceneFactValue(Node, "second", 2);
	const auto Handle = InTarget.AddNode(Node);
	FSceneEditDocument Document;
	Document.Attach(InTarget);
	Document.Selection() = Handle;
	InScene.Acknowledge(InTarget.Revision());
	const auto Revision = InTarget.Revision();
	TSceneComponentBatchRequest<FSceneFactTestValue> Request{Document.Id(), Revision, {Handle}, {"second"}, {{3}}};
	SetSceneComponentBatch(Document, Request);
	CheckSceneFacts(InScene, Handle, {{"test.scene-fact", "second", Modified}});
	CheckSceneFact(Document.GetState().HistoryCursor == 1 && Document.IsDirty());
	InScene.Acknowledge(InTarget.Revision());
	Document.Undo();
	CheckSceneFacts(InScene, Handle, {{"test.scene-fact", "second", Modified}});
	CheckSceneFact(Document.GetState().HistoryCursor == 0 && !Document.IsDirty());
	InScene.Acknowledge(InTarget.Revision());
	Document.Redo();
	CheckSceneFacts(InScene, Handle, {{"test.scene-fact", "second", Modified}});
	CheckSceneFact(InTarget.Revision() == Revision + 3 && Document.Selection().Primary() == Handle);
	InScene.Acknowledge(InTarget.Revision());
	EditSceneComponentStructure(Document,
	                            {Document.Id(), InTarget.Revision(), {Handle}, "point", "hyperion.scenepointlight"});
	CheckSceneFacts(InScene, Handle, {{"hyperion.scenepointlight", "point", Added}});
	Document.Undo();
	CheckSceneFacts(InScene, Handle, {{"hyperion.scenepointlight", "point", Added | Removed}});
	CheckSceneFact(!InTarget.FindNode(Handle)->PointLight());
	Document.Redo();
	CheckSceneFacts(InScene, Handle, {{"hyperion.scenepointlight", "point", Added | Removed}});
	CheckSceneFact(InTarget.FindNode(Handle)->PointLight().has_value());
	InScene.Acknowledge(InTarget.Revision());
	Document.Undo();
	CheckSceneFacts(InScene, Handle, {{"hyperion.scenepointlight", "point", Removed}});
	InScene.Acknowledge(InTarget.Revision());
	Document.Redo();
	CheckSceneFacts(InScene, Handle, {{"hyperion.scenepointlight", "point", Added}});
	InScene.Acknowledge(InTarget.Revision());
	const auto Duplicate = Document.CommitDuplicate(Handle);
	CheckSceneFacts(InScene, Duplicate,
	                {{"hyperion.scenecamera", "hyperion.scenecamera", Added},
	                 {"hyperion.scenepointlight", "point", Added},
	                 {"hyperion.scenetransform", "hyperion.scenetransform", Added},
	                 {"test.scene-fact", "first", Added},
	                 {"test.scene-fact", "second", Added}});
	Document.Detach(InTasks);
}

void CheckKeepChildrenDomainFacts(FTaskSystem& InTasks, ISceneEditTarget& InTarget, FScene& InScene)
{
	FSceneNode ParentNode;
	ParentNode.Id = "facts-parent";
	ParentNode.Local() = Translation({5, 0, 0});
	const auto Parent = InTarget.AddNode(ParentNode);
	auto ChildNode = MakeSceneCameraNode("facts-child");
	ChildNode.Parent() = ParentNode.Id;
	const auto Child = InTarget.AddNode(ChildNode);
	InTarget.SetSettings({Child, {}});
	FSceneEditDocument Document;
	Document.Attach(InTarget);
	Document.Selection() = Parent;
	InScene.Acknowledge(InTarget.Revision());
	Document.CommitRemoveKeepChildren(Parent);
	CheckSceneFacts(InScene, Parent, {{"hyperion.scenetransform", "hyperion.scenetransform", Removed}});
	CheckSceneFacts(InScene, Child, {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	InScene.Acknowledge(InTarget.Revision());
	Document.Undo();
	const auto Restored = InTarget.FindHandle(ParentNode.Id);
	CheckSceneFact(Restored.Generation != Parent.Generation);
	CheckSceneFacts(InScene, Restored, {{"hyperion.scenetransform", "hyperion.scenetransform", Added}});
	CheckSceneFacts(InScene, Child, {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	CheckSceneFact(Document.GetState().HistoryCursor == 0 && Document.Selection().Primary() == Restored &&
	               !Document.IsDirty());
	InScene.Acknowledge(InTarget.Revision());
	Document.Redo();
	CheckSceneFacts(InScene, Restored, {{"hyperion.scenetransform", "hyperion.scenetransform", Removed}});
	CheckSceneFacts(InScene, Child, {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	CheckSceneFact(Document.GetState().HistoryCursor == 1 && !Document.Selection() && Document.IsDirty());
	CheckSceneFact(InTarget.Settings().DefaultCamera == Child);
	Document.Detach(InTasks);
}

void CheckHistoryEntry(const FSceneHistoryEntry& InOld, const FSceneHistoryEntry& InCurrent)
{
	CheckSceneFact(InOld.Handle == InCurrent.Handle && InOld.Before == InCurrent.Before &&
	               InOld.After == InCurrent.After && InOld.BeforeSettings == InCurrent.BeforeSettings &&
	               InOld.AfterSettings == InCurrent.AfterSettings && InOld.BeforeState == InCurrent.BeforeState &&
	               InOld.AfterState == InCurrent.AfterState && InOld.DeletedSubtree == InCurrent.DeletedSubtree &&
	               InOld.DeletedRoots == InCurrent.DeletedRoots && InOld.BeforeSelection == InCurrent.BeforeSelection &&
	               InOld.bKeepChildren == InCurrent.bKeepChildren &&
	               InOld.bRestoreSelection == InCurrent.bRestoreSelection &&
	               InOld.CreatedNodes == InCurrent.CreatedNodes && InOld.CreatedRoots == InCurrent.CreatedRoots &&
	               InOld.AfterSelection == InCurrent.AfterSelection && InOld.Edits.size() == InCurrent.Edits.size());
	for (std::size_t Index = 0; Index < InOld.Edits.size(); ++Index)
	{
		const auto& Old = InOld.Edits[Index];
		const auto& Current = InCurrent.Edits[Index];
		CheckSceneFact(Old.Handle == Current.Handle && Old.Before == Current.Before && Old.After == Current.After);
	}
}

void CheckDocumentState(const FSceneDocumentState& InOld, const FSceneDocumentState& InCurrent)
{
	CheckSceneFact(InOld.HistoryCursor == InCurrent.HistoryCursor && InOld.NextState == InCurrent.NextState &&
	               InOld.State == InCurrent.State && InOld.SavedState == InCurrent.SavedState &&
	               InOld.Epoch == InCurrent.Epoch && InOld.Path == InCurrent.Path &&
	               InOld.History.size() == InCurrent.History.size());
	CheckSceneFact(!InOld.Interaction && !InCurrent.Interaction && !InOld.Save && !InCurrent.Save);
	for (std::size_t Index = 0; Index < InOld.History.size(); ++Index)
	{
		CheckHistoryEntry(InOld.History[Index], InCurrent.History[Index]);
	}
}

void CheckHistoryPreparationFailure(FTaskSystem& InTasks, ISceneEditTarget& InTarget, FScene& InScene, unsigned InCase)
{
	const auto Handle = AddSceneFactQueryModel(InScene);
	auto Node = *InTarget.FindNode(Handle);
	AddSceneFactValue(Node, "value", 1);
	InScene.EditNode(Handle, Node, InTarget.Revision());
	FSceneEditDocument Document;
	Document.Attach(InTarget);
	Document.Selection() = Handle;
	Node.Name =
	    "history-preparation-failure"; // Skips document and Scene whole-node Equal before the shared preparation.
	Node.Local() = Translation({1, 0, 0});
	SetSceneFactValue(Node, "value", 2);
	if (InCase > 0)
	{
		Document.CommitEdits({{Handle, Node}}, InTarget.Revision());
	}
	if (InCase == 2)
	{
		Document.Undo();
	}
	const auto Before = SnapshotSceneFacts(InScene);
	const auto State = Document.GetState();
	const auto Selection = Document.Selection();
	const bool bDirty = Document.IsDirty();
	SceneFactFault = {.EqualValue = InCase == 1 ? 2 : 1};
	bool bRejected{};
	try
	{
		if (InCase == 0)
		{
			Document.CommitEdits({{Handle, Node}}, InTarget.Revision());
		}
		else if (InCase == 1)
		{
			Document.Undo();
		}
		else
		{
			Document.Redo();
		}
	}
	catch (const std::runtime_error& Error)
	{
		bRejected = std::string_view(Error.what()) == "Scene fact Equal failure";
	}
	const auto EqualCalls = SceneFactFault.EqualCalls;
	SceneFactFault = {};
	CheckSceneFact(bRejected && EqualCalls == 1);
	CheckSceneFactSnapshot(InScene, Before);
	CheckDocumentState(State, Document.GetState());
	CheckSceneFact(Document.Selection() == Selection && Document.IsDirty() == bDirty);
	Document.Detach(InTasks);
}
} // namespace

void CheckSceneComponentDomainCase(FTaskSystem& InTasks, ISceneEditTarget& InTarget, FScene& InScene, unsigned InCase)
{
	RegisterSceneFactTypes();
	if (InCase == 0)
	{
		CheckComponentDomainFacts(InTasks, InTarget, InScene);
	}
	else if (InCase == 1)
	{
		CheckKeepChildrenDomainFacts(InTasks, InTarget, InScene);
	}
	else
	{
		CheckHistoryPreparationFailure(InTasks, InTarget, InScene, InCase - 2);
	}
}
