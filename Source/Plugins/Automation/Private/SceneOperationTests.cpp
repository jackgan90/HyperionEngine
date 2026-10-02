#include "Hyperion/Automation/Endpoint.h"
#include "Hyperion/Automation/Session.h"
#include "Hyperion/SceneEditing/SceneComponentEditing.h"
#include "Hyperion/SceneEditing/ScenePlacement.h"
#include "SceneOperations.h"
#include "SceneTestTarget.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <source_location>

namespace
{
using namespace Hyperion;
using FTarget = FSceneTestTarget;

void Check(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error("Scene document check failed at " + std::to_string(InLocation.line()));
	}
}

bool SameVector(FVec3 InLeft, FVec3 InRight)
{
	return InLeft.X == InRight.X && InLeft.Y == InRight.Y && InLeft.Z == InRight.Z;
}

const FArchiveNode& Field(const FArchiveNode& InValue, const char* InKey)
{
	return std::get<FArchiveNode::FObject>(InValue.Value).at(InKey);
}

template<class T> FArchiveNode Call(FAutomationSession& InSession, const char* InOperation, const T& InRequest)
{
	return InSession.Call(InOperation, WriteRecordWire(RecordType<T>(), &InRequest));
}

void Error(const FArchiveNode& InResult, const char* InCode)
{
	Check(ReadValue<std::string>(Field(Field(InResult, "error"), "code")) == InCode);
}

void Editing()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneNode Node;
	Node.Id = "group";
	const auto Handle = Target.AddNode(Node);
	FSceneEditDocument Document;
	Document.Attach(Target);
	Document.Selection() = Handle;
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	FAutomationSession OtherAgent(Catalog);
	const auto Original = Target.FindNode(Handle)->Local();
	FSceneTransformRequest Request{Document.Id(), Target.Revision(), {{Handle, Translation({2, 3, 4})}}};
	Check(ReadValue<std::string>(Field(Call(Agent, "scene.nodes.set_transform", Request), "status")) == "completed");
	Check(Document.IsDirty() && Document.GetState().HistoryCursor == 1);
	Check(Document.Selection().Primary() == Handle);
	Error(Call(OtherAgent, "scene.nodes.set_transform", Request), "stale_revision");
	Document.Undo(); // GUI and agents consume exactly the same history.
	Check(!Document.IsDirty() && Target.FindNode(Handle)->Local().Values == Original.Values);
	Document.Redo();
	Document.SetInteractionState(true, false);
	Request.Revision = Target.Revision();
	Error(Call(Agent, "scene.nodes.set_transform", Request), "busy");
	Document.SetInteractionState(false, false);
	const auto Before = Target.FindNode(Handle)->Local();
	Request.Transforms.push_back({FSceneHandle{Handle.Scene, Handle.Slot, Handle.Generation + 1}, Identity()});
	Error(Call(Agent, "scene.nodes.set_transform", Request), "stale_handle");
	Check(Target.FindNode(Handle)->Local().Values == Before.Values && Document.GetState().HistoryCursor == 1);
	Request.Transforms.pop_back();
	Request.Transforms.front().Local.Values[15] = 0;
	Error(Call(Agent, "scene.nodes.set_transform", Request), "invalid_arguments");
	Check(Target.FindNode(Handle)->Local().Values == Before.Values);
	const auto Page = ListSceneNodes(Document, {Document.Id(), Target.Revision(), 0, 1});
	Check(Page.Nodes.size() == 1 && Page.Total == 1 && !Page.Next);
	const auto Saved =
	    Call(Agent, "scene.save", FSceneSaveRequest{Document.Id(), Target.Revision(), "Captured.hasset"});
	Check(ReadValue<std::string>(Field(Saved, "status")) == "running");
	const auto Job = ReadValue<std::string>(Field(Saved, "job"));
	FAutomationEndpoint OtherEndpoint(OtherAgent);
	Error(OtherEndpoint.Execute("jobs.get", FArchiveNode(FArchiveNode::FObject{{"job", WriteValue(Job)}})),
	      "not_found");
	auto Later = *Target.FindNode(Handle);
	Later.Local() = Translation({7, 8, 9});
	Document.CommitEdits({{Handle, Later}}, Target.Revision());
	Agent.StopAdmission();
	Tasks.PumpMain();
	Agent.Poll();
	Check(!Agent.PendingCount() && Document.IsDirty() && !Document.GetState().Save);
	Document.Undo();
	Check(!Document.IsDirty()); // Captured save point, rather than completion-time state.
	Document.Redo();
	Document.CommitDelete();
	Check(!Document.Selection() && !Target.FindNode(Handle));
	Document.Undo();
	const auto Restored = Document.Selection().Primary();
	Check(Restored && *Restored != Handle && Target.FindNode(*Restored));
	const auto OldDocument = Document.Id();
	Document.Reset();
	Error(Call(OtherAgent, "scene.nodes.list", FSceneListRequest{OldDocument, Target.Revision()}), "stale_document");
	Document.Detach(Tasks);
}

void NoHistory()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target, false);
	FSceneEditDocument OtherDocument;
	OtherDocument.Attach(Target, false);
	Check(Document.Id() != OtherDocument.Id());
	OtherDocument.Detach(Tasks);
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	Error(Call(Session, "scene.undo", FSceneMutationRequest{Document.Id(), Target.Revision()}), "unavailable");
	const auto ResourceNode = Target.AddNode({});
	Check(!Document.IsDirty()); // Renderer preparation revisions are not authored edits.
	const auto Copy = Document.CommitDuplicate(ResourceNode);
	Check(Copy.Scene && Document.IsDirty() && Document.GetState().History.empty());
	Document.Reset();
	Document.CommitReparent(Copy, ResourceNode, ESceneReparentMode::KeepWorld);
	Check(Document.IsDirty() && Target.FindNode(Copy)->Parent() == Target.FindNode(ResourceNode)->Id);
	Document.Reset();
	Document.CommitRemoveKeepChildren(ResourceNode);
	Check(Document.IsDirty() && Target.FindNode(Copy)->Parent().empty());
	Document.Reset();
	Document.CommitCreate({});
	Check(Document.IsDirty());
	Document.Detach(Tasks);
	FOperationCatalog Missing;
	RegisterSceneOperations(Missing, nullptr);
	RegisterRenderCapture(Missing, nullptr, nullptr);
	RegisterViewportOperations(Missing, nullptr, nullptr);
	Missing.Seal();
	FAutomationSession Absent(Missing);
	Error(Call(Absent, "scene.info", FSceneInfoRequest{}), "unavailable");
	Error(Call(Absent, "renderdoc.hud.get", FSceneInfoRequest{}), "unavailable");
	Error(Call(Absent, "view.frame_selection", FSceneMutationRequest{}), "unavailable");
}

void CheckSingleSceneChange(const FTarget& InTarget, FSceneHandle InHandle, ESceneChangeMask InMask)
{
	const auto Changes = InTarget.Scene.GetChanges();
	Check(Changes.size() == 1 && Changes[0].Handle == InHandle);
	Check(Changes[0].Revision == InTarget.Revision() && Changes[0].Mask == InMask);
}

void ComponentHistoryBaseline()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	const auto Handle = Target.AddNode(MakeSceneCameraNode("history-camera"));
	const auto Original = *Target.FindNode(Handle);
	FSceneEditDocument Document;
	Document.Attach(Target);
	Document.Selection() = Handle;
	const auto SavedState = Document.GetState().SavedState;
	Target.Scene.Acknowledge(Target.Revision());
	const auto Revision = Target.Revision();
	auto Lens = *Original.Camera();
	Lens.Far = 2000;
	TSceneComponentBatchRequest<FSceneCamera> Request{
	    Document.Id(), Revision, {Handle}, {RecordType<FSceneCamera>().Id}, {Lens}};
	SetSceneComponentBatch(Document, Request);
	Check(Target.Revision() == Revision + 1 && Document.GetState().HistoryCursor == 1 && Document.IsDirty());
	CheckSingleSceneChange(Target, Handle, ESceneChangeMask::Metadata | ESceneChangeMask::Camera);
	Check(Document.Selection().Primary() == Handle && Document.GetState().History.size() == 1);
	Check(Document.GetState().History[0].Edits[0].Before == Original);
	Check(Document.GetState().History[0].Edits[0].After.Camera() == Lens);
	const auto NextState = Document.GetState().NextState;
	Target.Scene.Acknowledge(Target.Revision());
	Document.Undo();
	CheckSingleSceneChange(Target, Handle, ESceneChangeMask::Metadata | ESceneChangeMask::Camera);
	Check(Target.Revision() == Revision + 2 && *Target.FindNode(Handle) == Original);
	Check(!Document.IsDirty() && Document.GetState().HistoryCursor == 0 && Document.GetState().State == SavedState);
	Target.Scene.Acknowledge(Target.Revision());
	Document.Redo();
	CheckSingleSceneChange(Target, Handle, ESceneChangeMask::Metadata | ESceneChangeMask::Camera);
	Check(Target.Revision() == Revision + 3 && Document.GetState().HistoryCursor == 1 && Document.IsDirty());
	Check(Document.GetState().NextState == NextState && Document.Selection().Primary() == Handle);
	Target.Scene.Acknowledge(Target.Revision());
	Request.Revision = Target.Revision();
	SetSceneComponentBatch(Document, Request);
	Check(Target.Revision() == Revision + 3 && Target.Scene.GetChanges().empty());
	Check(Document.GetState().HistoryCursor == 1 && Document.GetState().NextState == NextState);
	const auto BeforeState = Document.GetState().State;
	Request.Values[0].Far = -1;
	bool bRejected{};
	try
	{
		SetSceneComponentBatch(Document, Request);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	Check(bRejected && Target.Revision() == Revision + 3 && Target.Scene.GetChanges().empty());
	Check(Target.FindNode(Handle)->Camera() == Lens && Document.GetState().State == BeforeState);
	Check(Document.GetState().HistoryCursor == 1 && Document.GetState().History.size() == 1);
	Check(Document.GetState().NextState == NextState && Document.GetState().SavedState == SavedState);
	Check(Document.Selection().Primary() == Handle && Document.IsDirty());
	const auto PointType = RecordType<FScenePointLight>().Id;
	EditSceneComponentStructure(Document, {Document.Id(), Target.Revision(), {Handle}, "point", PointType});
	CheckSingleSceneChange(Target, Handle, ESceneChangeMask::Metadata | ESceneChangeMask::Light);
	Check(Target.Revision() == Revision + 4);
	Check(Document.GetState().HistoryCursor == 2 && Target.FindNode(Handle)->PointLight());
	Target.Scene.Acknowledge(Target.Revision());
	Document.Undo();
	CheckSingleSceneChange(Target, Handle, ESceneChangeMask::Metadata | ESceneChangeMask::Light);
	Check(Target.Revision() == Revision + 5);
	Check(Document.GetState().HistoryCursor == 1 && !Target.FindNode(Handle)->PointLight());
	Target.Scene.Acknowledge(Target.Revision());
	Document.Redo();
	CheckSingleSceneChange(Target, Handle, ESceneChangeMask::Metadata | ESceneChangeMask::Light);
	Check(Target.Revision() == Revision + 6);
	Check(Document.GetState().HistoryCursor == 2 && Target.FindNode(Handle)->PointLight());
	Document.Detach(Tasks);
}

void KeepChildrenHistoryBaseline()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneNode ParentNode;
	ParentNode.Id = "parent";
	ParentNode.Local() = Translation({5, 0, 0});
	const auto Parent = Target.AddNode(ParentNode);
	auto ChildNode = MakeSceneCameraNode("child");
	ChildNode.Parent() = "parent";
	ChildNode.Local() = Translation({2, 0, 0});
	const auto Child = Target.AddNode(ChildNode);
	Target.SetSettings({Child, {}});
	FSceneEditDocument Document;
	Document.Attach(Target);
	Document.Selection() = Parent;
	Target.Scene.Acknowledge(Target.Revision());
	const auto Revision = Target.Revision();
	Document.CommitRemoveKeepChildren(Parent);
	Check(Target.Revision() == Revision + 1 && Target.Scene.GetChanges().size() == 2);
	Check(!Target.FindNode(Parent) && Target.FindNode(Child)->Parent().empty());
	Check(Target.FindNode(Child)->Local().Values[12] == 7 && Target.Settings().DefaultCamera == Child);
	Check(!Document.Selection() && Document.IsDirty() && Document.GetState().HistoryCursor == 1);
	for (const auto& Change : Target.Scene.GetChanges())
	{
		Check(Change.Mask == ESceneChangeMask::Structure);
	}
	Target.Scene.Acknowledge(Target.Revision());
	Document.Undo();
	const auto Restored = Target.FindHandle("parent");
	Check(Restored.Slot == Parent.Slot && Restored.Generation != Parent.Generation);
	Check(Target.Revision() == Revision + 2 && Target.Scene.GetChanges().size() == 2);
	Check(Document.Selection().Primary() == Restored && !Document.IsDirty());
	Check(Document.GetState().HistoryCursor == 0 && Document.GetState().History[0].Handle == Restored);
	Check(*Target.FindNode(Child) == ChildNode && Target.Settings().DefaultCamera == Child);
	for (const auto& Change : Target.Scene.GetChanges())
	{
		const auto Expected =
		    Change.Handle == Restored
		        ? ESceneChangeMask::Structure | ESceneChangeMask::Transform | ESceneChangeMask::Enabled
		        : ESceneChangeMask::Metadata | ESceneChangeMask::Structure | ESceneChangeMask::Transform;
		Check(Change.Mask == Expected);
	}
	Target.Scene.Acknowledge(Target.Revision());
	Document.Redo();
	Check(Target.Revision() == Revision + 3 && Target.FindNode(Child)->Local().Values[12] == 7);
	Check(!Target.FindNode(Restored) && !Document.Selection() && Document.GetState().HistoryCursor == 1);
	Target.Scene.Acknowledge(Target.Revision());
	const auto State = Document.GetState().State;
	const auto NextState = Document.GetState().NextState;
	Target.bRejectAdd = true;
	bool bRejected{};
	try
	{
		Document.Undo();
	}
	catch (const std::runtime_error&)
	{
		bRejected = true;
	}
	Target.bRejectAdd = false;
	Check(bRejected && Target.Revision() == Revision + 3 && Target.Scene.GetChanges().empty());
	Check(Document.GetState().HistoryCursor == 1 && Document.GetState().History[0].Handle == Restored);
	Check(Document.GetState().State == State && Document.GetState().NextState == NextState);
	Check(Document.IsDirty() && !Document.Selection() && Target.FindNode(Child)->Parent().empty());
	Check(Target.Scene.GetRoots() == std::vector{Child} && Target.Settings().DefaultCamera == Child);
	FSceneNodeView View;
	Check(Target.NodeView(Child, View) && View.World.Values[12] == 7 && View.bEffectiveEnabled);
	Document.Detach(Tasks);
}

void DocumentFailureSequenceBaseline()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	const auto Handle = Target.AddNode(MakeSceneCameraNode("failure-sequence"));
	FSceneEditDocument Document;
	Document.Attach(Target);
	Document.Selection() = Handle;
	auto Edited = *Target.FindNode(Handle);
	Edited.Name = "first interaction";
	Document.CommitEdits({{Handle, Edited}}, Target.Revision(), 31);
	Target.Scene.Acknowledge(Target.Revision());
	const auto Revision = Target.Revision();
	const auto State = Document.GetState().State;
	const auto NextState = Document.GetState().NextState;
	Check(Document.GetState().Interaction.has_value());
	FSceneNode Invalid;
	Invalid.Local().Values[15] = 0;
	bool bRejected{};
	try
	{
		Document.CommitCreate(Invalid);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	// Existing create behavior consumes a serial and ends the interaction before Scene admission.
	Check(bRejected && !Document.GetState().Interaction && Document.GetState().NextState == NextState + 1);
	Check(Target.Revision() == Revision && Target.Scene.GetChanges().empty() && *Target.FindNode(Handle) == Edited);
	Check(Document.GetState().State == State && Document.GetState().HistoryCursor == 1 && Document.IsDirty());
	Check(Document.Selection().Primary() == Handle);
	Edited.Name = "second interaction";
	Document.CommitEdits({{Handle, Edited}}, Target.Revision(), 32);
	Check(Document.GetState().Interaction.has_value());
	Target.Scene.RemoveSubtree(Handle);
	Target.Scene.Acknowledge(Target.Revision());
	const auto RemovedRevision = Target.Revision();
	const auto BeforeUndo = Document.GetState().State;
	const auto BeforeUndoSerial = Document.GetState().NextState;
	bRejected = false;
	try
	{
		Document.Undo();
	}
	catch (const FSceneEditError& Error)
	{
		bRejected = Error.Code == "stale_handle";
	}
	// Failed history restoration also ends the current interaction, without moving its committed cursor.
	Check(bRejected && !Document.GetState().Interaction && Document.GetState().HistoryCursor == 2);
	Check(Document.GetState().State == BeforeUndo && Document.GetState().NextState == BeforeUndoSerial);
	Check(Target.Revision() == RemovedRevision && Target.Scene.GetChanges().empty());
	Check(Document.Selection().Primary() == Handle && Document.IsDirty());
	Document.Detach(Tasks);
}

void ComponentAdmissionAndBatch()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	const auto First = CreateSceneNode(Document, {Document.Id(), Target.Revision(), "First"}).Handle;
	const auto Second = CreateSceneNode(Document, {Document.Id(), Target.Revision(), "Second"}).Handle;
	const auto Revision = Target.Revision();
	const auto History = Document.GetState().HistoryCursor;
	for (const auto& Type : {RecordType<FSceneModelComponent>().Id, RecordType<FSceneModelSource>().Id})
	{
		Error(Call(Agent, "scene.node.create",
		           FSceneCreateRequest{Document.Id(), Revision, "Invalid", {}, Identity(), {Type}}),
		      "invalid_arguments");
		Error(Call(Agent, "scene.components.edit_structure",
		           FSceneComponentStructureRequest{Document.Id(), Revision, {First}, Type, Type, false}),
		      "invalid_arguments");
		Check(Target.Revision() == Revision && Document.GetState().HistoryCursor == History);
	}
	EditSceneComponentStructure(Document,
	                            {Document.Id(), Target.Revision(), {First}, "camera-a", RecordType<FSceneCamera>().Id});
	EditSceneComponentStructure(
	    Document, {Document.Id(), Target.Revision(), {Second}, "camera-b", RecordType<FSceneCamera>().Id});
	FSceneCamera CameraA;
	FSceneCamera CameraB;
	CameraA.Far = 2000;
	CameraB.Far = 3000;
	SetSceneComponentBatch(
	    Document, TSceneComponentBatchRequest<FSceneCamera>{
	                  Document.Id(), Target.Revision(), {First, Second}, {"camera-a", "camera-b"}, {CameraA, CameraB}});
	CameraA.Near = CameraB.Near = .5f;
	TSceneComponentBatchRequest<FSceneCamera> Batch{
	    Document.Id(), Target.Revision(), {First, Second}, {"camera-a", "camera-b"}, {CameraA, CameraB}};
	const auto Operation = "scene.component." + RecordType<FSceneCamera>().Id + ".set_batch";
	const auto BeforeBatch = Document.GetState().HistoryCursor;
	Check(ReadValue<std::string>(
	          Field(Agent.Call(Operation, WriteRecordWire(SceneComponentBatchRequestType<FSceneCamera>(), &Batch)),
	                "status")) == "completed");
	Check(Target.FindNode(First)->Camera()->Near == .5f && Target.FindNode(Second)->Camera()->Near == .5f);
	Check(Target.FindNode(First)->Camera()->Far == 2000 && Target.FindNode(Second)->Camera()->Far == 3000);
	Check(Document.GetState().HistoryCursor == BeforeBatch + 1);
	Document.Undo();
	Check(Target.FindNode(First)->Camera()->Near == FSceneCamera{}.Near &&
	      Target.FindNode(Second)->Camera()->Near == FSceneCamera{}.Near);
	Batch.Revision = Target.Revision();
	Batch.Components[1] = "missing";
	Error(Agent.Call(Operation, WriteRecordWire(SceneComponentBatchRequestType<FSceneCamera>(), &Batch)), "not_found");
	Check(Target.Revision() == Batch.Revision && Document.GetState().HistoryCursor == BeforeBatch);
	Batch.Components[1] = "camera-b";
	Batch.Values.pop_back();
	Error(Agent.Call(Operation, WriteRecordWire(SceneComponentBatchRequestType<FSceneCamera>(), &Batch)),
	      "invalid_arguments");
	Check(Target.Revision() == Batch.Revision);
	Document.Detach(Tasks);
}

void StructuralHistory()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneNode Parent;
	Parent.Id = "parent";
	Parent.Camera() = FSceneCamera{};
	Parent.Local() = Translation({4, 1, 0});
	const auto Root = Target.AddNode(Parent);
	FSceneNode Child;
	Child.Id = "child";
	Child.Parent() = Parent.Id;
	Child.Local() = Translation({2, 3, 0});
	const auto Leaf = Target.AddNode(Child);
	Target.Scene.SetSettings({Root, {}});
	FSceneEditDocument Document;
	Document.Attach(Target);
	Document.ReplaceSelection(FSceneSelection(Root));
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	const auto Result =
	    Call(Agent, "scene.selection.duplicate", FSceneMutationRequest{Document.Id(), Target.Revision()});
	Check(ReadValue<std::string>(Field(Result, "status")) == "completed");
	const auto Copy = *Document.Selection().Primary();
	Check(Copy != Root && Target.Children(Copy).empty());
	Document.Undo();
	Check(!Target.FindNode(Copy) && Document.Selection().Primary() == Root);
	Document.Redo();
	Check(Document.Selection().Primary() != Copy && Target.Nodes().size() == 3);
	Document.Undo();
	FSceneNodeView Before;
	Target.NodeView(Leaf, Before);
	const auto World = Before.World;
	const auto Removed =
	    Call(Agent, "scene.selection.remove_keep_children", FSceneMutationRequest{Document.Id(), Target.Revision()});
	Check(ReadValue<std::string>(Field(Removed, "status")) == "completed");
	Check(!Target.FindNode(Root) && !Target.Settings().DefaultCamera && !Document.Selection());
	FSceneNodeView After;
	Target.NodeView(Leaf, After);
	Check(After.World.Values == World.Values && After.Node->Parent().empty());
	for (unsigned Index = 0; Index < 3; ++Index)
	{
		Document.Undo();
		const auto Restored = Target.FindHandle("parent");
		Check(Target.FindNode(Leaf)->Parent() == "parent" &&
		      Target.FindNode(Leaf)->Local().Values == Child.Local().Values);
		Check(Document.Selection().Primary() == Restored && Target.Settings().DefaultCamera == Restored);
		Document.Redo();
		Check(Target.Nodes().size() == 1 && !Target.Settings().DefaultCamera);
	}
	Document.Detach(Tasks);
}

void SkyLightActivation()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	Check(WriteJson(Catalog.Search("scene.sky.use_default")).find("scene.sky.use_default") == std::string::npos);
	const auto Type = RecordType<FSceneEnvironmentLight>().Id;
	const auto Create = [&](const char* InName)
	{
		const auto Created =
		    Call(Agent, "scene.node.create",
		         FSceneCreateRequest{Document.Id(), Target.Revision(), InName, {}, Identity(), {Type}});
		Check(ReadValue<std::string>(Field(Created, "status")) == "completed");
		return Document.Selection().Primary().value();
	};
	const auto First = Create("First");
	const auto& Created = *Target.FindNode(First)->EnvironmentLight();
	Check(Target.Scene.GetLightingSelection().Environment.Handle == First &&
	      Created.Source == ESceneEnvironmentSource::SkyAsset);
	Check(Created.Sky == DefaultSkyReference() && SameVector(Created.Tint, {1, 1, 1}) && Created.bVisible);
	const auto Second = Create("Second");
	Check(Target.Scene.GetLightingSelection().Environment.bTied);
	const auto Holder = CreateSceneNode(Document, {Document.Id(), Target.Revision(), "Holder"}).Handle;
	EditSceneComponentStructure(Document, {Document.Id(), Target.Revision(), {Holder}, "sky", Type});
	Check(Target.Scene.GetLightingSelection().Environment.bTied && Target.FindNode(Holder)->EnvironmentLight());

	const auto Previous = Target.Scene.GetLightingSelection().Environment.Handle;
	auto Edited = *Target.FindNode(Second);
	Edited.EnvironmentLight()->Priority = 10;
	Document.CommitEdits({{Second, Edited}}, Target.Revision());
	Check(Target.Scene.GetLightingSelection().Environment.Handle == Second);
	Document.Undo();
	Check(Target.Scene.GetLightingSelection().Environment.Handle == Previous);
	Document.Redo();
	Check(Target.Scene.GetLightingSelection().Environment.Handle == Second);

	// Removing the winner automatically selects the remaining highest priority candidate.
	SetSceneSelection(Document, {Document.Id(), Target.Revision(), {Second}});
	DeleteSceneSelection(Document, {Document.Id(), Target.Revision()});
	Check(Target.Scene.GetLightingSelection().Environment.Handle != Second);
	EditSceneComponentStructure(Document, {Document.Id(), Target.Revision(), {Holder}, "sky", Type, true});
	const auto History = Document.GetState().HistoryCursor;
	EditSceneComponentStructure(Document, {Document.Id(), Target.Revision(), {Holder}, "sky", Type});
	Check(Target.Scene.GetLightingSelection().Environment.bTied && Document.GetState().HistoryCursor == History + 1);
	Document.Undo();
	Check(Target.Scene.GetLightingSelection().Environment.Handle == First &&
	      !Target.FindNode(Holder)->EnvironmentLight());
	Document.Redo();
	Check(Target.Scene.GetLightingSelection().Environment.bTied);

	// Invalid values leave the light and history unchanged. C++ records reject them on write, so forge the wire.
	const auto Operation = "scene.component." + Type + ".set";
	const auto Reject = [&](const char* InField, std::initializer_list<std::pair<const char*, FArchiveNode>> InValues)
	{
		TSceneComponentRequest<FSceneEnvironmentLight> Request{
		    Document.Id(), Target.Revision(), {First}, Type, *Target.FindNode(First)->EnvironmentLight()};
		auto Wire = WriteRecordWire(SceneComponentRequestType<FSceneEnvironmentLight>(), &Request);
		auto& Value = std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Wire.Value).at("value").Value);
		for (const auto& [Key, Replacement] : InValues)
		{
			std::get<FArchiveNode::FObject>(Value.at(InField).Value).at(Key) = Replacement;
		}
		const auto Before = *Target.FindNode(First)->EnvironmentLight();
		const auto Cursor = Document.GetState().HistoryCursor;
		Error(Agent.Call(Operation, Wire), "invalid_arguments");
		Check(*Target.FindNode(First)->EnvironmentLight() == Before && Document.GetState().HistoryCursor == Cursor);
	};
	const auto Empty = WriteValue(std::string());
	Reject("sky", {{"id", Empty}, {"path", Empty}});
	Reject("sky", {{"type", WriteValue(RecordType<FSceneCamera>().Id)}});
	Reject("tint", {{"y", WriteValue(-1.0)}});
	TSceneComponentRequest<FSceneEnvironmentLight> Tinted{
	    Document.Id(), Target.Revision(), {First}, Type, *Target.FindNode(First)->EnvironmentLight()};
	Tinted.Value.Tint = {1, 0, 0};
	Tinted.Value.YawDegrees = 90;
	Agent.Call(Operation, WriteRecordWire(SceneComponentRequestType<FSceneEnvironmentLight>(), &Tinted));
	Check(SameVector(Target.FindNode(First)->EnvironmentLight()->Tint, {1, 0, 0}));
	Check(Target.FindNode(First)->EnvironmentLight()->YawDegrees == 90);
	Document.Detach(Tasks);
}

FMat4 NodeWorld(const FTarget& InTarget, FSceneHandle InHandle)
{
	FSceneNodeView View;
	Check(InTarget.NodeView(InHandle, View));
	return View.World;
}

void CheckWorld(const FTarget& InTarget, FSceneHandle InHandle, const FMat4& InExpected)
{
	const auto Actual = NodeWorld(InTarget, InHandle);
	for (std::size_t Index = 0; Index < Actual.Values.size(); ++Index)
	{
		Check(std::abs(Actual.Values[Index] - InExpected.Values[Index]) < .0001f);
	}
}

void BatchReparent()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	Check(WriteJson(Catalog.Search("scene.nodes.reparent")).find("scene.nodes.reparent") != std::string::npos);
	Check(WriteJson(Catalog.Describe("scene.nodes.reparent")).find("handles") != std::string::npos);
	const auto Add = [&](std::string InName, std::optional<FSceneHandle> InParent, FMat4 InLocal)
	{
		return CreateSceneNode(Document, {Document.Id(), Target.Revision(), InName, InParent, InLocal}).Handle;
	};
	const auto A = Add("A", {}, Translation({2, 1, 0}));
	const auto B = Add("B", A, Scale({-2, .5f, 3}));
	const auto C = Add("C", {}, Translation({-1, 3, 4}));
	auto ParentLocal = ComposeTRS({4, 2, -3}, {0, std::sin(.3f), 0, std::cos(.3f)}, {-2, .5f, 3});
	ParentLocal.Values[4] += .3f;
	const auto Parent = Add("Parent", {}, ParentLocal);
	const std::vector Handles{B, A, C};
	SetSceneSelection(Document, {Document.Id(), Target.Revision(), Handles});
	const auto BeforeSelection = Document.Selection();
	const std::array Worlds{NodeWorld(Target, A), NodeWorld(Target, B), NodeWorld(Target, C)};
	const auto ChildLocal = Target.FindNode(B)->Local();
	const auto History = Document.GetState().HistoryCursor;
	const auto Revision = Target.Revision();
	const FSceneNodesReparentRequest Request{Document.Id(), Revision, Handles, Parent};
	Check(ReadValue<std::string>(Field(Call(Agent, "scene.nodes.reparent", Request), "status")) == "completed");
	Check(Target.Revision() == Revision + 1 && Document.GetState().HistoryCursor == History + 1);
	Check(Document.Selection() == BeforeSelection && Target.FindNode(B)->Parent() == Target.FindNode(A)->Id);
	Check(Target.FindNode(B)->Local().Values == ChildLocal.Values);
	CheckWorld(Target, A, Worlds[0]);
	CheckWorld(Target, B, Worlds[1]);
	CheckWorld(Target, C, Worlds[2]);
	Document.Undo();
	Check(Target.FindNode(A)->Parent().empty() && Target.FindNode(C)->Parent().empty());
	Document.Redo();
	Check(Document.Selection() == BeforeSelection && Target.FindNode(A)->Parent() == Target.FindNode(Parent)->Id);
	const auto NoopRevision = Target.Revision();
	ReparentSceneNodes(Document, {Document.Id(), NoopRevision, Handles, Parent});
	Check(Target.Revision() == NoopRevision && Document.GetState().HistoryCursor == History + 1);
	Error(Call(Agent, "scene.nodes.reparent", Request), "stale_revision");
	ReparentSceneNodes(Document, {Document.Id(), Target.Revision(), Handles, {}});
	Check(Target.FindNode(A)->Parent().empty() && Target.FindNode(C)->Parent().empty());
	CheckWorld(Target, A, Worlds[0]);
	CheckWorld(Target, B, Worlds[1]);
	CheckWorld(Target, C, Worlds[2]);
	Document.Detach(Tasks);
}

void AffineReparentRounding()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	const auto World = Translation({2, -3, 4});
	const auto Source = CreateSceneNode(Document, {Document.Id(), Target.Revision(), "Source", {}, World}).Handle;
	const auto Parent =
	    CreateSceneNode(Document, {Document.Id(), Target.Revision(), "Parent", {}, Scale({.05f, .7f, 1.3f})}).Handle;
	const auto History = Document.GetState().HistoryCursor;
	for (const bool bBatch : {false, true})
	{
		if (bBatch)
		{
			ReparentSceneNodes(Document, {Document.Id(), Target.Revision(), {Source}, Parent});
		}
		else
		{
			ReparentSceneNode(Document, {Document.Id(), Target.Revision(), Source, Parent, true});
		}
		Check(Target.FindNode(Source)->Parent() == Target.FindNode(Parent)->Id);
		Check(IsAffine(Target.FindNode(Source)->Local()));
		CheckWorld(Target, Source, World);
		Check(Document.GetState().HistoryCursor == History + 1);
		Document.Undo();
		Check(Target.FindNode(Source)->Parent().empty());
		CheckWorld(Target, Source, World);
	}
	Document.Detach(Tasks);
}

void BatchReparentRejection()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	const auto A = CreateSceneNode(Document, {Document.Id(), Target.Revision(), "A"}).Handle;
	const auto B = CreateSceneNode(Document, {Document.Id(), Target.Revision(), "B", A}).Handle;
	const auto Singular =
	    CreateSceneNode(Document, {Document.Id(), Target.Revision(), "Singular", {}, Scale({1, 0, 1})}).Handle;
	Document.Reset();
	const auto Revision = Target.Revision();
	const auto Reject =
	    [&](std::vector<FSceneHandle> InHandles, std::optional<FSceneHandle> InParent, const char* InCode)
	{
		Error(Call(Agent, "scene.nodes.reparent",
		           FSceneNodesReparentRequest{Document.Id(), Revision, InHandles, InParent}),
		      InCode);
		Check(Target.Revision() == Revision && Document.GetState().History.empty() && !Document.IsDirty());
		Check(Target.FindNode(A)->Parent().empty() && Target.FindNode(B)->Parent() == Target.FindNode(A)->Id);
	};
	Reject({A}, A, "invalid_arguments");
	Reject({A}, B, "invalid_arguments");
	Reject({A, B}, B, "invalid_arguments");
	Reject({A, A}, {}, "invalid_arguments");
	Reject({}, {}, "invalid_arguments");
	Reject({A, {A.Scene, A.Slot, A.Generation + 1}}, Singular, "stale_handle");
	Reject({A}, FSceneHandle{}, "stale_handle");
	Reject({A, B}, Singular, "invalid_arguments");
	Document.SetInteractionState(true, false);
	Reject({A}, {}, "busy");
	Document.SetInteractionState(false, false);
	Error(Call(Agent, "scene.nodes.reparent", FSceneNodesReparentRequest{"old", Revision, {A}, {}}), "stale_document");
	Document.Detach(Tasks);
}

void LargeBatchReparent()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	std::vector<FSceneNode> Nodes;
	for (unsigned Index = 0; Index < 256; ++Index)
	{
		FSceneNode Node;
		Node.Id = "batch-" + std::to_string(Index);
		Node.Local() = Translation({float(Index), 2, 3});
		Nodes.push_back(std::move(Node));
	}
	const auto Handles = Target.AddNodes(Nodes);
	const auto Parent = CreateSceneNode(Document, {Document.Id(), Target.Revision(), "Parent"}).Handle;
	const auto History = Document.GetState().HistoryCursor;
	ReparentSceneNodes(Document, {Document.Id(), Target.Revision(), Handles, Parent});
	Check(Document.GetState().HistoryCursor == History + 1);
	for (std::size_t Index = 0; Index < Handles.size(); ++Index)
	{
		Check(Target.FindNode(Handles[Index])->Parent() == Target.FindNode(Parent)->Id);
		CheckWorld(Target, Handles[Index], Nodes[Index].Local());
	}
	Document.Undo();
	for (const auto Handle : Handles)
	{
		Check(Target.FindNode(Handle)->Parent().empty());
	}
	Document.Detach(Tasks);
}

void Authoring()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	const auto Created =
	    Call(Agent, "scene.node.create",
	         FSceneCreateRequest{
	             Document.Id(), Target.Revision(), "Camera", {}, Identity(), {RecordType<FSceneCamera>().Id}});
	Check(ReadValue<std::string>(Field(Created, "status")) == "completed");
	const auto Handle = Document.Selection().Primary().value();
	Check(Target.FindNode(Handle)->Camera().has_value());
	const auto Parent = CreateSceneNode(Document, {Document.Id(), Target.Revision(), "Parent"}).Handle;
	Call(Agent, "scene.selection.set", FSceneSelectionRequest{Document.Id(), Target.Revision(), {Parent, Handle}});
	Check(Document.Selection().Primary() == Handle && Document.Selection().All().size() == 2);
	const auto History = Document.GetState().HistoryCursor;
	Error(
	    Call(Agent, "scene.selection.set", FSceneSelectionRequest{Document.Id(), Target.Revision(), {Parent, Parent}}),
	    "invalid_arguments");
	Check(Document.Selection().Primary() == Handle && Document.GetState().HistoryCursor == History);
	Call(
	    Agent, "scene.nodes.set_metadata",
	    FSceneMetadataRequest{Document.Id(), Target.Revision(), {{Parent, "Parent renamed", {}}, {Handle, {}, false}}});
	Check(Target.FindNode(Parent)->Name == "Parent renamed" && !Target.FindNode(Handle)->bEnabled);
	Document.Undo();
	Check(Target.FindNode(Parent)->Name == "Parent" && Target.FindNode(Handle)->bEnabled);
	Call(Agent, "scene.node.reparent", FSceneReparentRequest{Document.Id(), Target.Revision(), Handle, Parent, true});
	Check(Target.FindNode(Handle)->Parent() == Target.FindNode(Parent)->Id);
	const auto Revision = Target.Revision();
	Error(Call(Agent, "scene.node.reparent", FSceneReparentRequest{Document.Id(), Revision, Parent, Handle, true}),
	      "invalid_arguments");
	Check(Target.Revision() == Revision);
	TSceneComponentRequest<FSceneCamera> Lens{
	    Document.Id(), Target.Revision(), {Handle}, RecordType<FSceneCamera>().Id, {}};
	Lens.Value.Far = 2000;
	const auto Operation = "scene.component." + RecordType<FSceneCamera>().Id + ".set";
	Check(ReadValue<std::string>(Field(
	          Agent.Call(Operation, WriteRecordWire(SceneComponentRequestType<FSceneCamera>(), &Lens)), "status")) ==
	      "completed");
	Check(Target.FindNode(Handle)->Camera()->Far == 2000);
	Document.Undo();
	Check(Target.FindNode(Handle)->Camera()->Far == 1000);
	Lens.Revision = Target.Revision();
	Lens.Handles.push_back(Parent);
	Error(Agent.Call(Operation, WriteRecordWire(SceneComponentRequestType<FSceneCamera>(), &Lens)), "not_found");
	Check(Target.FindNode(Handle)->Camera()->Far == 1000);
	Call(Agent, "scene.selection.delete", FSceneMutationRequest{Document.Id(), Target.Revision()});
	Check(Target.Nodes().empty() && !Document.Selection());
	Document.Undo();
	Check(Target.Nodes().size() == 2 && Document.Selection().All().size() == 2);
	Document.Detach(Tasks);
}

void SelectAll()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	Check(WriteJson(Catalog.Search("scene.selection.select_all")).find("scene.selection.select_all") !=
	      std::string::npos);
	Check(WriteJson(Catalog.Describe("scene.selection.select_all")).find("hyperion.scene.selection.summary") !=
	      std::string::npos);
	const auto Empty = SelectAllSceneNodes(Document, {Document.Id(), Target.Revision()});
	Check(Empty.Count == 0 && !Empty.Primary && !Document.IsDirty());
	for (unsigned Index = 0; Index < 300; ++Index)
	{
		FSceneNode Node;
		Node.Id = "selection-" + std::to_string(Index);
		Node.bEnabled = Index % 2 != 0;
		Node.Parent() = Index ? "selection-0" : "";
		Target.AddNode(std::move(Node));
	}
	const auto Handles = Target.Nodes();
	const auto Revision = Target.Revision();
	Document.ReplaceSelection(FSceneSelection(Handles.front()));
	const FSceneMutationRequest Request{Document.Id(), Revision};
	const auto Result = Call(Agent, "scene.selection.select_all", Request);
	Check(ReadValue<std::string>(Field(Result, "status")) == "completed");
	Check(ReadValue<std::string>(Field(Field(Result, "result"), "count")) == std::to_string(Handles.size()));
	Check(Document.Selection().All().size() == Handles.size() && Document.Selection().Primary() == Handles.front());
	Check(WriteJson(Result).size() < 1000);
	Check(Target.Revision() == Revision && !Document.IsDirty() && Document.GetState().History.empty());
	Call(Agent, "scene.selection.set", FSceneSelectionRequest{Document.Id(), Revision, Handles});
	Check(Document.Selection().All() == Handles);
	const auto Before = Document.Selection();
	auto Invalid = Handles;
	Invalid.push_back(Handles.front());
	Error(Call(Agent, "scene.selection.set", FSceneSelectionRequest{Document.Id(), Revision, Invalid}),
	      "invalid_arguments");
	Invalid.back().Generation += 1;
	Error(Call(Agent, "scene.selection.set", FSceneSelectionRequest{Document.Id(), Revision, Invalid}), "stale_handle");
	Invalid.back() = {Target.Identity() + 1, 0, 1};
	Error(Call(Agent, "scene.selection.set", FSceneSelectionRequest{Document.Id(), Revision, Invalid}), "stale_handle");
	Error(Call(Agent, "scene.selection.select_all", FSceneMutationRequest{Document.Id(), Revision - 1}),
	      "stale_revision");
	Error(Call(Agent, "scene.selection.select_all", FSceneMutationRequest{"old-document", Revision}), "stale_document");
	Document.SetInteractionState(true, false);
	Error(Call(Agent, "scene.selection.select_all", Request), "busy");
	Document.SetInteractionState(false, false);
	Check(Document.Selection() == Before && Target.Revision() == Revision && !Document.IsDirty());
	Document.Detach(Tasks);
}

void ValidationAndRootCosts()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	std::vector<FSceneHandle> Handles;
	std::string Parent;
	for (std::size_t Index = 0; Index < 256; ++Index)
	{
		auto Node = MakeScenePointLightNode({});
		Node.Parent() = Parent;
		const auto Handle = Target.AddNode(std::move(Node));
		Handles.push_back(Handle);
		Parent = Target.FindNode(Handle)->Id;
	}
	const auto Handle = Handles.back();
	const auto Component = RecordType<FScenePointLight>().Id;
	Target.NodesCount = 0;
	GetSceneSettings(Document, {Document.Id(), Target.Revision()});
	GetSceneSelection(Document, {Document.Id(), Target.Revision()});
	GetSceneComponent(Document, {Document.Id(), Target.Revision(), Handle, Component});
	ListSceneComponents(Document, {Document.Id(), Handle});
	Check(Target.NodesCount == 0);
	const auto Value =
	    *static_cast<const FScenePointLight*>(Target.FindNode(Handle)->Components.Find(Component)->Get());
	const std::vector Batch(Handles.begin(), Handles.begin() + 64);
	SetSceneComponent(Document, {Document.Id(), Target.Revision(), Batch}, Component, RecordType<FScenePointLight>(),
	                  &Value);
	Check(Target.NodesCount == 1); // Only the result's nodeCount query enumerates.
	TSceneComponentBatchRequest<FScenePointLight> Typed{Document.Id(), Target.Revision(), Batch};
	for (std::size_t Index = 0; Index < Batch.size(); ++Index)
	{
		Typed.Components.push_back(Component);
		auto Item = Value;
		Item.Intensity = static_cast<float>(Index + 1);
		Typed.Values.push_back(Item);
	}
	SetSceneComponentBatch(Document, Typed);
	Check(Target.NodesCount == 2);
	Document.ReplaceSelection(FSceneSelection{});
	for (const auto Selected : Handles)
	{
		Document.Selection().Toggle(Selected);
	}
	Target.FindHandleCount = 0;
	Check(Document.SelectedRoots() == std::vector{Handles.front()});
	Check(Target.FindHandleCount == Handles.size() - 1);
	Target.FindHandleCount = 0;
	Check(Document.PrepareReparent(Handles, {}).empty());
	Check(Target.FindHandleCount == Handles.size() - 1);
	const auto Revision = Target.Revision();
	const auto History = Document.GetState().HistoryCursor;
	auto Invalid = Batch;
	Invalid.push_back({Handle.Scene, Handle.Slot, Handle.Generation + 1});
	auto Different = Value;
	Different.Intensity = 900.f;
	const auto BeforeValue =
	    *static_cast<const FScenePointLight*>(Target.FindNode(Batch.front())->Components.Find(Component)->Get());
	try
	{
		SetSceneComponent(Document, {Document.Id(), Revision, Invalid}, Component, RecordType<FScenePointLight>(),
		                  &Different);
		Check(false);
	}
	catch (const FSceneEditError& Failure)
	{
		Check(Failure.Code == "stale_handle");
	}
	Check(Target.Revision() == Revision && Document.GetState().HistoryCursor == History);
	Check(*static_cast<const FScenePointLight*>(Target.FindNode(Batch.front())->Components.Find(Component)->Get()) ==
	      BeforeValue);
	FSceneNode Independent;
	const auto Other = Target.AddNode(std::move(Independent));
	SetSceneSelection(Document, {Document.Id(), Target.Revision(), {Other, Handles.back(), Handles.front()}});
	Check(Document.SelectedRoots() == std::vector{Other, Handles.front()});
	Document.Detach(Tasks);
}

void SelectionWithoutProvider()
{
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, nullptr);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	Check(!Catalog.Find("scene.selection.select_all").Info.Unavailable.empty());
	Error(Call(Agent, "scene.selection.select_all", FSceneMutationRequest{"absent-document", 1}), "unavailable");
}

void PlacementWithoutProvider()
{
	FOperationCatalog Catalog;
	RegisterPlacementOperations(Catalog, nullptr);
	Catalog.Seal();
	Check(!Catalog.Find("scene.placement.place_model").Info.Unavailable.empty());
	FAutomationSession Agent(Catalog);
	const FSceneModelPlacementRequest Request{
	    "absent-document", 1, {"", "/Game/Model.hasset", RecordType<FModelAsset>().Id, ""}, {}};
	Error(Call(Agent, "scene.placement.place_model", Request), "unavailable");
}
} // namespace

void CheckSceneClipboardOperations();
void CheckSceneComponentDomainChanges();
void CheckSceneRegistrationOperations();
void CheckRenderOptionOperations();

int main()
{
	try
	{
		Editing();
		CheckSceneComponentDomainChanges();
		ComponentHistoryBaseline();
		KeepChildrenHistoryBaseline();
		DocumentFailureSequenceBaseline();
		ValidationAndRootCosts();
		PlacementWithoutProvider();
		CheckSceneClipboardOperations();
		CheckSceneRegistrationOperations();
		CheckRenderOptionOperations();
		NoHistory();
		Authoring();
		SelectAll();
		SelectionWithoutProvider();
		BatchReparent();
		AffineReparentRounding();
		BatchReparentRejection();
		LargeBatchReparent();
		SkyLightActivation();
		StructuralHistory();
		ComponentAdmissionAndBatch();
		std::cout << "Shared scene, identity, transactions, history, save and absence contracts passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
