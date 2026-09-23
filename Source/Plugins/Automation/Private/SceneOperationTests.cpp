#include "Hyperion/Automation/Endpoint.h"
#include "Hyperion/Automation/Session.h"
#include "Hyperion/SceneEditing/SceneComponentEditing.h"
#include "SceneOperations.h"
#include <iostream>
#include <source_location>

namespace
{
using namespace Hyperion;

void Check(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error("Scene document check failed at " + std::to_string(InLocation.line()));
	}
}

class FTarget final : public ISceneEditTarget
{
public:
	explicit FTarget(FTaskSystem& InTasks) : Tasks(InTasks)
	{
	}

	FTaskSystem& Tasks;
	FScene Scene;

	bool IsLoaded() const override
	{
		return true;
	}

	bool IsReady() const override
	{
		return true;
	}

	std::uint64_t Identity() const override
	{
		return Scene.GetIdentity();
	}

	std::uint64_t Revision() const override
	{
		return Scene.GetRevision();
	}

	const FSceneNode* FindNode(FSceneHandle InHandle) const override
	{
		return Scene.FindNode(InHandle);
	}

	FSceneHandle FindHandle(std::string_view InId) const override
	{
		return Scene.FindHandle(InId);
	}

	std::vector<FSceneHandle> Nodes() const override
	{
		return Scene.GetNodes();
	}

	std::vector<FSceneHandle> Children(FSceneHandle InHandle) const override
	{
		return Scene.GetChildren(InHandle);
	}

	bool NodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const override
	{
		return Scene.GetNodeView(InHandle, OutView);
	}

	const FSceneSettings& Settings() const override
	{
		return Scene.GetSettings();
	}

	bool EditNodes(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision) override
	{
		return Scene.EditNodes(std::move(InEdits), InExpectedRevision);
	}

	FSceneHandle AddNode(FSceneNode InNode) override
	{
		return Scene.AddNode(std::move(InNode));
	}

	FSceneHandle DuplicateNode(FSceneHandle InHandle) override
	{
		const auto* Source = Scene.FindNode(InHandle);
		if (!Source)
		{
			return {};
		}
		auto Node = *Source;
		Node.Id.clear();
		return Scene.AddNode(std::move(Node));
	}

	bool RemoveNodeKeepChildren(FSceneHandle InHandle) override
	{
		return Scene.RemoveNodeKeepChildren(InHandle);
	}

	std::vector<FSceneHandle> AddNodes(std::vector<FSceneNode> InNodes) override
	{
		return Scene.AddNodes(std::move(InNodes));
	}

	bool RemoveSubtrees(std::span<const FSceneHandle> InRoots) override
	{
		return Scene.RemoveSubtrees(InRoots);
	}

	void SetSettings(FSceneSettings InSettings) override
	{
		Scene.SetSettings(std::move(InSettings));
	}

	FSceneNode Rebind(FSceneNode InNode) override
	{
		return InNode;
	}

	void RefreshAssets() override
	{
	}

	TAsyncResult<bool> Save(const std::filesystem::path&) override
	{
		return DispatchAsync<bool>(Tasks, {EDomain::Main},
		                           []
		                           {
			                           return true;
		                           });
	}
};

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

int main()
{
	try
	{
		Editing();
		NoHistory();
		Authoring();
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
