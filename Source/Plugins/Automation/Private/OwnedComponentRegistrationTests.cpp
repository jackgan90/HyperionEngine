#include "Hyperion/Automation/Session.h"
#include "Hyperion/AutomationHost/SceneComponentOperations.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Hyperion/Serialization/Archive.h"
#include "SceneTestTarget.h"
#include <source_location>
#include <thread>

namespace Hyperion
{
struct FOwnedTestComponent
{
	int Strength{};
	std::string Tag = "initial";
	bool operator==(const FOwnedTestComponent&) const = default;
};

template<> const FRecordDescriptor& RecordType<FOwnedTestComponent>()
{
	static const auto Type = MakeRecord<FOwnedTestComponent>(
	    "test.owned-component",
	    {Member("strength", &FOwnedTestComponent::Strength), Member("tag", &FOwnedTestComponent::Tag)}, 1,
	    [](const FOwnedTestComponent& InValue)
	    {
		    if (InValue.Strength < 1 || InValue.Strength > 100)
		    {
			    throw std::invalid_argument("Strength must be 1..100");
		    }
	    });
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;
constexpr std::string_view Owner = "owned-component-test";
constexpr std::string_view GetId = "scene.component.test.owned-component.get";
constexpr std::string_view SetId = "scene.component.test.owned-component.set";
constexpr std::string_view BatchId = "scene.component.test.owned-component.set_batch";

void Check(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error("Owned component registration check failed at " + std::to_string(InLocation.line()));
	}
}

template<class TFunction> void Rejected(TFunction InFunction)
{
	bool bRejected{};
	try
	{
		InFunction();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	Check(bRejected);
}

const FArchiveNode& Field(const FArchiveNode& InNode, std::string_view InKey)
{
	return std::get<FArchiveNode::FObject>(InNode.Value).at(std::string(InKey));
}

void Error(const FArchiveNode& InResponse, std::string_view InCode)
{
	Check(ReadValue<std::string>(Field(Field(InResponse, "error"), "code")) == InCode);
}

FSceneComponentOperationOptions Options(bool bInWrite = true)
{
	return {std::string(Owner), true, bInWrite};
}

template<class TRequest>
FArchiveNode Call(FAutomationSession& InSession, std::string_view InId, const FRecordDescriptor& InType,
                  const TRequest& InRequest)
{
	return InSession.Call(InId, WriteRecordWire(InType, &InRequest));
}

void RegistrationPreflight()
{
	FOperationCatalog Catalog;
	Rejected(
	    [&]
	    {
		    RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, Options(), {1});
	    });
	Check(Catalog.Size() == 0);
	auto Component = MakeSceneComponent<FOwnedTestComponent>("Owned test component");
	Component.bUnique = false;
	SceneComponentRegistry().Register(std::move(Component));
	Rejected(
	    [&]
	    {
		    RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, Options());
	    });
	Check(Catalog.Size() == 0); // Invalid default examples never publish the earlier get or batch member.
	Rejected(
	    [&]
	    {
		    RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, {});
	    });
	RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, Options(false));
	Check(Catalog.Size() == 1 && Catalog.Find(GetId).Info.bReadOnly);
	Rejected(
	    [&]
	    {
		    (void)Catalog.Find(SetId);
	    });
	Rejected(
	    [&]
	    {
		    (void)Catalog.Find(BatchId);
	    });
	Rejected(
	    [&]
	    {
		    RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, Options(), {1});
	    });
	Check(Catalog.Size() == 1); // Batch is considered before get, but a duplicate get adds nothing.
	Catalog.UnregisterOwner(Owner);
	RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, Options(), {1});
	Check(Catalog.Size() == 3);
	for (const auto Id : {GetId, SetId, BatchId})
	{
		const auto& Operation = Catalog.Find(Id);
		Check(Operation.Info.Owner == Owner && Operation.Info.Version == 1 && !Operation.bAsynchronous);
		Check(WriteJson(Field(Catalog.Describe(Id), "inputSchema")) == WriteJson(RecordWireSchema(*Operation.Request)));
		bool bUnavailable{};
		const auto Example = ReadRecordWire(*Operation.Request, Operation.Info.Example);
		try
		{
			(void)Operation.Invoke(Example.get());
		}
		catch (const FAutomationError& Failure)
		{
			bUnavailable = Failure.Code == AutomationErrors::Unavailable;
		}
		Check(bUnavailable);
	}
	Catalog.Seal();
	Rejected(
	    [&]
	    {
		    RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, Options(), {1});
	    });
	Check(Catalog.Size() == 3);
	FAutomationSession Session(Catalog);
	Error(Session.Call(GetId, Catalog.Find(GetId).Info.Example), "unavailable");
	Catalog.UnregisterOwner(Owner);
	Check(Catalog.Size() == 0);
	Error(Session.Call(GetId, FArchiveNode(FArchiveNode::FObject{})), "not_found");
}

void OwnershipAndTypePreflight()
{
	FOperationCatalog Catalog;
	bool bWrongThreadRejected{};
	std::thread Other(
	    [&]
	    {
		    try
		    {
			    RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, Options(), {1});
		    }
		    catch (const std::logic_error&)
		    {
			    bWrongThreadRejected = true;
		    }
	    });
	Other.join();
	Check(bWrongThreadRejected && Catalog.Size() == 0);
	auto DetachedType = RecordType<FOwnedTestComponent>();
	Rejected(
	    [&]
	    {
		    RegisterSceneComponentOperationFamily(Catalog, DetachedType, {});
	    });
	Rejected(
	    [&]
	    {
		    RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, {std::string(Owner), false, false},
		                                                          {1});
	    });
	const auto Conflict = MakeRecord<FSceneInfoRequest>(RecordType<FOwnedTestComponent>().Id, {});
	Catalog.RegisterType(Conflict);
	Rejected(
	    [&]
	    {
		    RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, nullptr, Options(), {1});
	    });
	Check(Catalog.Size() == 0);
}

std::vector<FSceneHandle> Seed(FSceneTestTarget& InTarget)
{
	std::vector<FSceneHandle> Handles;
	for (const auto* Id : {"owned-first", "owned-second"})
	{
		FSceneNode Node;
		Node.Id = Id;
		Node.Components.Add("custom", RecordType<FOwnedTestComponent>().Id);
		*static_cast<FOwnedTestComponent*>(Node.Components.Find("custom")->Edit()) = {2};
		Handles.push_back(InTarget.AddNode(std::move(Node)));
	}
	InTarget.Scene.Acknowledge(InTarget.Revision());
	return Handles;
}

std::vector<std::byte> Persisted(const FSceneTestTarget& InTarget)
{
	FSceneManifest Manifest;
	for (const auto Handle : InTarget.Nodes())
	{
		Manifest.Nodes.push_back(SceneEntryFromNode(*InTarget.FindNode(Handle)));
	}
	const auto Archive = WriteRecord(RecordType<FSceneManifest>(), &Manifest);
	const auto Restored = ReadRecord(RecordType<FSceneManifest>(), DecodeArchive(EncodeArchive(Archive)));
	const auto& Loaded = *static_cast<const FSceneManifest*>(Restored.get());
	const auto Nodes = NodesFromSceneManifest(Loaded);
	Check(Nodes.size() == 2);
	for (std::size_t Index = 0; Index < Nodes.size(); ++Index)
	{
		const auto* Value = Nodes[Index].Components.Find("custom");
		Check(Value && *static_cast<const FOwnedTestComponent*>(Value->Get()) ==
		                   *static_cast<const FOwnedTestComponent*>(
		                       InTarget.FindNode(InTarget.Nodes()[Index])->Components.Find("custom")->Get()));
	}
	return EncodeArchive(Archive);
}

void Equivalent(const FSceneTestTarget& InDomain, const FSceneEditDocument& InDomainDocument,
                const FSceneTestTarget& InAgent, const FSceneEditDocument& InAgentDocument)
{
	Check(Persisted(InDomain) == Persisted(InAgent));
	Check(InDomain.Revision() == InAgent.Revision());
	Check(InDomainDocument.IsDirty() == InAgentDocument.IsDirty());
	Check(InDomainDocument.GetState().HistoryCursor == InAgentDocument.GetState().HistoryCursor);
	Check(InDomainDocument.GetState().History.size() == InAgentDocument.GetState().History.size());
	const auto DomainChanges = InDomain.Scene.GetChanges();
	const auto AgentChanges = InAgent.Scene.GetChanges();
	Check(DomainChanges.size() == AgentChanges.size());
	for (std::size_t Index = 0; Index < DomainChanges.size(); ++Index)
	{
		Check(DomainChanges[Index].ComponentChanges == AgentChanges[Index].ComponentChanges);
	}
}

void DomainEquivalence()
{
	FTaskSystem Tasks(1, 1);
	FSceneTestTarget Domain(Tasks);
	FSceneTestTarget Agent(Tasks);
	const auto DomainHandles = Seed(Domain);
	const auto AgentHandles = Seed(Agent);
	FSceneEditDocument DomainDocument;
	FSceneEditDocument AgentDocument;
	DomainDocument.Attach(Domain);
	AgentDocument.Attach(Agent);
	FOperationCatalog Catalog;
	RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, &AgentDocument, Options(), {1});
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	const FSceneComponentRequest Get{AgentDocument.Id(), Agent.Revision(), AgentHandles[0], "custom"};
	const auto Read = Call(Session, GetId, RecordType<FSceneComponentRequest>(), Get);
	Check(ReadValue<std::string>(Field(Read, "status")) == "completed");
	Check(ReadValue<int>(Field(Field(Read, "result"), "strength")) == 2);
	TSceneComponentRequest<FOwnedTestComponent> Set{
	    AgentDocument.Id(), Agent.Revision(), {AgentHandles[0]}, "custom", {3}};
	SetSceneComponent(DomainDocument, {DomainDocument.Id(), Domain.Revision(), {DomainHandles[0]}}, "custom",
	                  RecordType<FOwnedTestComponent>(), &Set.Value);
	Check(ReadValue<std::string>(Field(Call(Session, SetId, SceneComponentRequestType<FOwnedTestComponent>(), Set),
	                                   "status")) == "completed");
	Equivalent(Domain, DomainDocument, Agent, AgentDocument);
	Domain.Scene.Acknowledge(Domain.Revision());
	Agent.Scene.Acknowledge(Agent.Revision());
	TSceneComponentBatchRequest<FOwnedTestComponent> Batch{
	    AgentDocument.Id(), Agent.Revision(), AgentHandles, {"custom", "custom"}, {{4, "first"}, {5, "second"}}};
	auto DomainBatch = Batch;
	DomainBatch.Document = DomainDocument.Id();
	DomainBatch.Revision = Domain.Revision();
	DomainBatch.Handles = DomainHandles;
	SetSceneComponentBatch(DomainDocument, DomainBatch);
	Check(ReadValue<std::string>(
	          Field(Call(Session, BatchId, SceneComponentBatchRequestType<FOwnedTestComponent>(), Batch), "status")) ==
	      "completed");
	Equivalent(Domain, DomainDocument, Agent, AgentDocument);
	DomainDocument.Undo();
	AgentDocument.Undo();
	Equivalent(Domain, DomainDocument, Agent, AgentDocument);
	DomainDocument.Redo();
	AgentDocument.Redo();
	Equivalent(Domain, DomainDocument, Agent, AgentDocument);
	Catalog.UnregisterOwner(Owner);
	AgentDocument.Detach(Tasks);
	Error(Call(Session, GetId, RecordType<FSceneComponentRequest>(), Get), "not_found");
	DomainDocument.Detach(Tasks);
}

void AtomicRejection()
{
	FTaskSystem Tasks(1, 1);
	FSceneTestTarget Target(Tasks);
	const auto Handles = Seed(Target);
	FSceneEditDocument Document;
	Document.Attach(Target);
	FOperationCatalog Catalog;
	RegisterSceneComponentOperations<FOwnedTestComponent>(Catalog, &Document, Options(), {1});
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	TSceneComponentBatchRequest<FOwnedTestComponent> Request{
	    Document.Id(), Target.Revision(), Handles, {"custom", "missing"}, {{4}, {5}}};
	const auto Before = Persisted(Target);
	const auto Revision = Target.Revision();
	Error(Call(Session, BatchId, SceneComponentBatchRequestType<FOwnedTestComponent>(), Request), "not_found");
	Request.Components = {"custom", "custom"};
	auto InvalidValue = WriteRecordWire(SceneComponentBatchRequestType<FOwnedTestComponent>(), &Request);
	auto& Values =
	    std::get<FArchiveNode::FArray>(std::get<FArchiveNode::FObject>(InvalidValue.Value).at("values").Value);
	std::get<FArchiveNode::FObject>(Values[1].Value).at("strength") = WriteValue(101);
	Error(Session.Call(BatchId, InvalidValue), "invalid_arguments");
	++Request.Revision;
	Error(Call(Session, BatchId, SceneComponentBatchRequestType<FOwnedTestComponent>(), Request), "stale_revision");
	Request.Revision = Revision;
	++Request.Handles[1].Generation;
	Error(Call(Session, BatchId, SceneComponentBatchRequestType<FOwnedTestComponent>(), Request), "stale_handle");
	const FSceneComponentRequest WrongType{Document.Id(), Revision, Handles[0], RecordType<FSceneTransform>().Id};
	Error(Call(Session, GetId, RecordType<FSceneComponentRequest>(), WrongType), "invalid_arguments");
	Check(Persisted(Target) == Before && Target.Revision() == Revision && !Document.IsDirty());
	Check(Document.GetState().History.empty() && Target.Scene.GetChanges().empty());
	Catalog.UnregisterOwner(Owner);
	Document.Detach(Tasks);
}
} // namespace

void CheckOwnedComponentRegistration()
{
	RegistrationPreflight();
	OwnershipAndTypePreflight();
	DomainEquivalence();
	AtomicRejection();
}
