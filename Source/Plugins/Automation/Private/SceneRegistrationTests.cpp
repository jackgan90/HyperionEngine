#include "Hyperion/Automation/Session.h"
#include "Hyperion/SceneEditing/SceneComponentEditing.h"
#include "SceneOperations.h"
#include "SceneTestTarget.h"
#include <source_location>

namespace
{
using namespace Hyperion;

void Check(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error("Scene registration check failed at " + std::to_string(InLocation.line()));
	}
}

const FArchiveNode& Field(const FArchiveNode& InValue, const char* InKey)
{
	return std::get<FArchiveNode::FObject>(InValue.Value).at(InKey);
}

void CheckSchemas(const FOperationCatalog& InCatalog, const char* InId, const FRecordDescriptor& InRequest,
                  const FRecordDescriptor& InResult, bool bInReadOnly, const char* InCompletion)
{
	const auto& Operation = InCatalog.Find(InId);
	Check(Operation.Request == &InRequest && Operation.Result == &InResult);
	Check(Operation.Info.Owner == "automation-scene" && Operation.Info.Version == 1);
	Check(Operation.Info.bReadOnly == bInReadOnly && !Operation.bAsynchronous);
	Check(Operation.Info.Completion == InCompletion && !Operation.Info.Effects.empty());
	const auto Description = InCatalog.Describe(InId);
	Check(WriteJson(Field(Description, "inputSchema")) == WriteJson(RecordWireSchema(InRequest)));
	Check(WriteJson(Field(Description, "outputSchema")) == WriteJson(RecordWireSchema(InResult)));
	Check(WriteJson(Field(Description, "example")) == WriteJson(Operation.Info.Example));
	Check(WriteJson(InCatalog.Search(InId)).find(InId) != std::string::npos);
}

void SceneMetadata()
{
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, nullptr);
	Catalog.Seal();
	CheckSchemas(Catalog, "scene.nodes.list", RecordType<FSceneListRequest>(), RecordType<FSceneNodePage>(), true,
	             "Main state updated; renderer publication occurs on a subsequent frame. No disk write unless "
	             "explicitly saving.");
	CheckSchemas(Catalog, "scene.settings.get", RecordType<FSceneMutationRequest>(), RecordType<FSceneSettings>(), true,
	             "Main state committed; rendering observes the change on subsequent frames.");
	CheckSchemas(Catalog, "scene.selection.copy", RecordType<FSceneMutationRequest>(),
	             RecordType<FSceneClipboardInfo>(), false,
	             "Completed on Main. Scene rendering observes committed edits on subsequent frames.");
	CheckSchemas(Catalog, "scene.component.hyperion.scenecamera.get", RecordType<FSceneComponentRequest>(),
	             RecordType<FSceneCamera>(), true, "Main state committed; does not wait for rendering or write disk.");
	CheckSchemas(Catalog, "scene.component.hyperion.scenecamera.set", SceneComponentRequestType<FSceneCamera>(),
	             RecordType<FSceneDocumentInfo>(), false,
	             "Main state committed; does not wait for rendering or write disk.");
	CheckSchemas(Catalog, "scene.component.hyperion.scenecamera.set_batch",
	             SceneComponentBatchRequestType<FSceneCamera>(), RecordType<FSceneDocumentInfo>(), false,
	             "Main state committed; does not wait for rendering or write disk.");
	Check(Catalog.Find("scene.nodes.list").Info.Keywords ==
	      std::vector<std::string>({"scene", "live", "transform", "document"}));
	Check(Catalog.Find("scene.settings.get").Info.Description ==
	      "Reads the default runtime camera and saved initial browsing view.");
	Check(Catalog.Find("scene.selection.copy").Info.Effects ==
	      "Replaces the system clipboard with an object token and name text. Does not change scene or history.");
	FAutomationSession Session(Catalog);
	for (const auto* Id :
	     {"scene.nodes.list", "scene.settings.get", "scene.selection.copy", "scene.component.hyperion.scenecamera.get",
	      "scene.component.hyperion.scenecamera.set", "scene.component.hyperion.scenecamera.set_batch"})
	{
		const auto& Operation = Catalog.Find(Id);
		Check(!Operation.Info.Unavailable.empty());
		const auto Result = Session.Call(Id, Operation.Info.Example);
		Check(ReadValue<std::string>(Field(Field(Result, "error"), "code")) == "unavailable");
	}
	Check(Catalog.Find("scene.component_types.list").Info.Unavailable.empty());
	Check(ReadValue<std::string>(
	          Field(Session.Call("scene.component_types.list", FArchiveNode(FArchiveNode::FObject{})), "status")) ==
	      "completed");
}

void ComponentInvocation()
{
	FTaskSystem Tasks(1, 1);
	FSceneTestTarget Target(Tasks);
	FSceneNode Node;
	Node.Camera() = FSceneCamera{};
	const auto Handle = Target.AddNode(Node);
	FSceneEditDocument Document;
	Document.Attach(Target);
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &Document);
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	const auto Get = [&](FSceneComponentRequest InRequest)
	{
		return Session.Call("scene.component.hyperion.scenecamera.get",
		                    WriteRecordWire(RecordType<FSceneComponentRequest>(), &InRequest));
	};
	const FSceneComponentRequest Request{Document.Id(), Target.Revision(), Handle, RecordType<FSceneCamera>().Id};
	const auto Result = Get(Request);
	Check(ReadValue<std::string>(Field(Result, "status")) == "completed");
	Check(WriteJson(Field(Result, "result")) ==
	      WriteJson(WriteRecordWire(RecordType<FSceneCamera>(), &*Target.FindNode(Handle)->Camera())));
	auto Stale = Request;
	Stale.Document = "stale-document";
	Check(ReadValue<std::string>(Field(Field(Get(Stale), "error"), "code")) == "stale_document");
	Stale = Request;
	++Stale.Handle.Generation;
	Check(ReadValue<std::string>(Field(Field(Get(Stale), "error"), "code")) == "stale_handle");
	const auto& Copy = Catalog.Find("scene.selection.copy");
	Check(!Copy.Info.Unavailable.empty());
	const auto MissingClipboard = Session.Call(Copy.Info.Id, Copy.Info.Example);
	Check(ReadValue<std::string>(Field(Field(MissingClipboard, "error"), "code")) == "unavailable");
	Document.Detach(Tasks);
}
} // namespace

void CheckSceneRegistrationOperations()
{
	SceneMetadata();
	ComponentInvocation();
}
