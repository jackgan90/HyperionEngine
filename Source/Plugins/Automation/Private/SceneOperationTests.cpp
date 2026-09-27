#include "Hyperion/Automation/Endpoint.h"
#include "Hyperion/Automation/Session.h"
#include "Hyperion/SceneEditing/SceneComponentEditing.h"
#include "SceneOperations.h"
#include "SceneTestTarget.h"
#include <cmath>
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
	Missing.Seal();
	FAutomationSession Absent(Missing);
	Error(Call(Absent, "scene.info", FSceneInfoRequest{}), "unavailable");
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
	Target.Scene.SetSettings({Root, {}, {}});
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

void DefaultSky()
{
	FTaskSystem Tasks(1, 1);
	FTarget Target(Tasks);
	FSceneEditDocument Document;
	Document.Attach(Target);
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Agent(Catalog);
	const FSceneMutationRequest Request{Document.Id(), Target.Revision()};
	Check(ReadValue<std::string>(Field(Call(Agent, "scene.sky.use_default", Request), "status")) == "completed");
	Check(Target.Nodes().size() == 1 && Document.GetState().HistoryCursor == 1 && Document.IsDirty());
	const auto First = *Target.Settings().EnvironmentLight;
	Check(Target.FindNode(First)->EnvironmentLight()->Sky == DefaultSkyReference());
	Error(Call(Agent, "scene.sky.use_default", Request), "stale_revision");
	Check(Target.Nodes().size() == 1 && Document.GetState().HistoryCursor == 1);
	Document.Undo();
	Check(Target.Nodes().empty() && !Target.Settings().EnvironmentLight && !Document.IsDirty());
	Document.Redo();
	const auto Active = *Target.Settings().EnvironmentLight;
	Check(Target.FindNode(Active)->EnvironmentLight()->Sky == DefaultSkyReference());
	auto Node = *Target.FindNode(Active);
	Node.bEnabled = false;
	Node.EnvironmentLight()->Source = ESceneEnvironmentSource::ConstantColor;
	Node.EnvironmentLight()->Sky.reset();
	Node.EnvironmentLight()->bVisible = false;
	Node.EnvironmentLight()->Intensity = .7f;
	Node.EnvironmentLight()->YawRadians = .4f;
	Document.CommitEdits({{Active, Node}}, Target.Revision());
	const auto History = Document.GetState().HistoryCursor;
	UseDefaultSceneSky(Document, {Document.Id(), Target.Revision()});
	const auto& Light = *Target.FindNode(Active)->EnvironmentLight();
	Check(Target.Nodes().size() == 1 && Document.GetState().HistoryCursor == History + 1);
	Check(Light.Sky == DefaultSkyReference() && Light.bVisible && Light.Intensity == .7f && Light.YawRadians == .4f);
	Check(Target.FindNode(Active)->bEnabled);
	Document.Undo();
	Check(*Target.FindNode(Active)->EnvironmentLight() == *Node.EnvironmentLight());
	Check(!Target.FindNode(Active)->bEnabled);
	Document.Redo();
	Document.SetInteractionState(true, false);
	Error(Call(Agent, "scene.sky.use_default", FSceneMutationRequest{Document.Id(), Target.Revision()}), "busy");
	Check(Document.GetState().HistoryCursor == History + 1);
	Document.SetInteractionState(false, false);
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
} // namespace

void CheckSceneClipboardOperations();

int main()
{
	try
	{
		Editing();
		CheckSceneClipboardOperations();
		NoHistory();
		Authoring();
		BatchReparent();
		AffineReparentRounding();
		BatchReparentRejection();
		LargeBatchReparent();
		DefaultSky();
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
