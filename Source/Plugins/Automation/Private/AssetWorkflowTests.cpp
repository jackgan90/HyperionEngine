#include "AssetOperations.h"
#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/Automation/Session.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <chrono>
#include <source_location>
#include <thread>

namespace
{
using namespace Hyperion;

void Check(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error("Asset workflow check failed at " + std::to_string(InLocation.line()));
	}
}

template<class TFunction> void Wait(FTaskSystem& InTasks, TFunction InFunction)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (!InFunction())
	{
		Check(std::chrono::steady_clock::now() < Deadline);
		InTasks.PumpMain();
		std::this_thread::yield();
	}
}

template<class T> T Finish(FTaskSystem& InTasks, TPendingOperation<T> InOperation)
{
	std::optional<T> Result;
	Wait(InTasks,
	     [&]
	     {
		     Result = InOperation.Poll();
		     return Result.has_value();
	     });
	return *Result;
}

const FArchiveNode& Field(const FArchiveNode& InNode, const char* InName)
{
	return std::get<FArchiveNode::FObject>(InNode.Value).at(InName);
}

std::string Text(const FArchiveNode& InNode, const char* InName)
{
	return ReadValue<std::string>(Field(InNode, InName));
}

FArchiveNode Outcome(FAutomationSession& InSession, FTaskSystem& InTasks, FArchiveNode InResult)
{
	if (Text(InResult, "status") != "running")
	{
		return InResult;
	}
	const auto Job = Text(InResult, "job");
	Wait(InTasks,
	     [&]
	     {
		     InResult = InSession.GetJob(Job);
		     return Text(InResult, "status") != "running";
	     });
	return Field(InResult, "outcome");
}

class FSharedWorkspace final : public IAssetWorkspace
{
public:
	explicit FSharedWorkspace(std::shared_ptr<FAssetEditDocument> InDocument)
	{
		Entry = FAssetWorkspaceEntry{"gui-document", InDocument->Loaded().Path, std::move(InDocument), false, true};
	}

	std::string OpenDocument(const std::filesystem::path&) override
	{
		return Entry->Id;
	}

	bool IsBlocked() const override
	{
		return false;
	}

	void PumpDocument(std::string_view) override
	{
	}

	std::optional<FAssetWorkspaceEntry> FindDocument(std::string_view InId) const override
	{
		if (!Entry || Entry->Id != InId)
		{
			return {};
		}
		auto Result = *Entry;
		Result.bEditing = Result.Document && Result.Document->IsEditing();
		return Result;
	}

	std::vector<FAssetWorkspaceEntry> Documents() const override
	{
		return Entry ? std::vector{*FindDocument(Entry->Id)} : std::vector<FAssetWorkspaceEntry>{};
	}

	void ActivateDocument(std::string_view) override
	{
	}

	void CloseDocument(std::string_view) override
	{
		Entry.reset();
		++Closed;
	}

	void SetExternalEditing(std::string_view, bool bInEditing) override
	{
		Entry->bEditing = bInEditing;
	}

	void Replace(std::shared_ptr<FAssetEditDocument> InDocument)
	{
		Entry->Document = std::move(InDocument);
	}

	std::optional<FAssetWorkspaceEntry> Entry;
	unsigned Closed{};
};

struct FWorkflowFixture
{
	FTaskSystem Tasks{2, 1};
	std::shared_ptr<FMemoryFileSystem> Files = std::make_shared<FMemoryFileSystem>();
	FIOService IO{Tasks, Files};
	FAssetService Assets{IO};
	FAssetRef Replacement;
	FAssetRef Broken;

	template<class T> FAssetRef Store(const char* InPath, const T& InAsset)
	{
		const auto Encoded = EncodeAsset(RecordType<T>(), &InAsset);
		Files->WriteAtomic(Assets.NormalizePath(InPath), Encoded.Bytes);
		return {Encoded.Header.Id, InPath, Encoded.Header.TypeId, {}};
	}

	FWorkflowFixture()
	{
		RegisterSceneAssetTypes(Assets.Types());
		const auto Texture =
		    BuildTextureAsset("Texture", EMaterialTextureEncoding::Linear,
		                      {2, 2, {0, 0, 0, 255, 255, 255, 255, 255, 128, 128, 128, 255, 64, 64, 64, 255}});
		const auto TextureRef = Store("workflow-texture.hasset", Texture);
		FMaterialAsset Material;
		Material.Name = "Material";
		FMaterialPass Pass;
		Pass.Vertex = {"Test.hlsl", "VSMain"};
		Pass.Pixel = {"Test.hlsl", "PSMain"};
		Material.Passes.push_back(Pass);
		const auto Original = Store("workflow-material.hasset", Material);
		Material.Name = "Replacement";
		Replacement = Store("workflow-replacement.hasset", Material);
		FMaterialAssetParameter Parameter;
		Parameter.Name = "Texture";
		Parameter.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
		Parameter.Default = FMaterialAssetValue{Parameter.Type};
		Parameter.Default->Texture = TextureRef;
		Parameter.Default->Texture->Path = "missing-texture.hasset";
		Material.Parameters.push_back(Parameter);
		Broken = Store("workflow-broken.hasset", Material);
		FModelAsset Model;
		Model.Name = "Model";
		Model.MaterialSlots.push_back(Original);
		FModelPrimitive Primitive;
		Primitive.Positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
		Primitive.Indices = {0, 1, 2};
		Primitive.Material = 0;
		Model.Primitives.push_back(Primitive);
		Model.Nodes.push_back({"Root", Identity(), {0}, {}, "node"});
		Model.Roots = {0};
		AssignModelSubresourceIds(Model);
		Store("workflow-model.hasset", Model);
	}

	std::shared_ptr<FAssetEditDocument> Document(const char* InPath)
	{
		return std::make_shared<FAssetEditDocument>(Assets.LoadAsync(InPath).Get(Tasks));
	}
};

void CheckEncodingParity()
{
	FWorkflowFixture F;
	auto Gui = F.Document("workflow-texture.hasset");
	auto AttachedDocument = F.Document("workflow-texture.hasset");
	FSharedWorkspace Workspace(AttachedDocument);
	FAssetAutomation Attached(F.Assets, F.Tasks, nullptr, &Workspace);
	FAssetAutomation Standalone(F.Assets, F.Tasks);
	const auto Initial = Gui->Snapshot();
	const auto Disk = F.Files->Read(Gui->Loaded().Path, 1024 * 1024);
	const auto State = Finish(F.Tasks, Standalone.Open({"workflow-texture.hasset"}));
	auto GuiWork = FAssetEditWorkflow::Encoding(F.Tasks, Gui, Gui->Generation(), EMaterialTextureEncoding::Srgb);
	auto AttachedWork =
	    Attached.SetEncoding({"gui-document", AttachedDocument->Generation(), EMaterialTextureEncoding::Srgb});
	auto StandaloneWork = Standalone.SetEncoding({State.Document, State.Generation, EMaterialTextureEncoding::Srgb});
	Check(Attached.Info({"gui-document"}).bEditing && Standalone.Info({State.Document}).bEditing);
	try
	{
		Attached.Close({"gui-document", AttachedDocument->Generation(), true});
		Check(false);
	}
	catch (const FAutomationError& Error)
	{
		Check(Error.Code == "busy");
	}
	Check(Workspace.Closed == 0);
	Wait(F.Tasks,
	     [&]
	     {
		     return GuiWork->Poll(Gui);
	     });
	const auto Changed = Finish(F.Tasks, std::move(StandaloneWork));
	const auto Shared = Finish(F.Tasks, std::move(AttachedWork));
	Check(!Shared.bEditing && !Changed.bEditing && Shared.bDirty && Changed.bDirty);
	Check(Shared.Generation == Gui->Generation() && Changed.Generation == Gui->Generation());
	Check(EqualInspectionValue(Gui->Snapshot(), AttachedDocument->Snapshot()));
	Check(EqualInspectionValue(Standalone.ReadField(State.Document, RecordType<FTextureAsset>().Id, "mips"),
	                           Gui->Get("mips")));
	Check(Gui->Undo() && AttachedDocument->Undo());
	const auto Undone = Standalone.Undo({Changed.Document, Changed.Generation});
	Check(!Gui->IsDirty() && !AttachedDocument->IsDirty() && !Undone.bDirty && !Undone.bCanUndo);
	Check(EqualInspectionValue(Gui->Snapshot(), Initial) &&
	      EqualInspectionValue(AttachedDocument->Snapshot(), Initial));
	Check(Gui->Redo() && AttachedDocument->Redo());
	const auto Redone = Standalone.Redo({Undone.Document, Undone.Generation});
	Check(Redone.bDirty && !Redone.bCanRedo && Gui->IsDirty() && AttachedDocument->IsDirty());
	Check(F.Files->Read(Gui->Loaded().Path, 1024 * 1024) == Disk);
}

void CheckReferenceParityAndFailure()
{
	FWorkflowFixture F;
	auto Gui = F.Document("workflow-model.hasset");
	auto Shared = F.Document("workflow-model.hasset");
	FSharedWorkspace Workspace(Shared);
	FAssetAutomation Attached(F.Assets, F.Tasks, nullptr, &Workspace);
	FAssetAutomation Standalone(F.Assets, F.Tasks);
	const auto State = Finish(F.Tasks, Standalone.Open({"workflow-model.hasset"}));
	const auto Before = Gui->Snapshot();
	const auto Disk = F.Files->Read(Gui->Loaded().Path, 1024 * 1024);
	const auto Value = WriteValue(std::vector{F.Replacement});
	auto GuiWork = FAssetEditWorkflow::Field(F.Tasks, F.Assets, Gui, Gui->Generation(), "materialSlots", Value);
	auto AttachedWork =
	    Attached.SetField("gui-document", Shared->Generation(), RecordType<FModelAsset>().Id, "materialSlots", Value);
	auto StandaloneWork =
	    Standalone.SetField(State.Document, State.Generation, RecordType<FModelAsset>().Id, "materialSlots", Value);
	Wait(F.Tasks,
	     [&]
	     {
		     return GuiWork->Poll(Gui);
	     });
	const auto Changed = Finish(F.Tasks, std::move(StandaloneWork));
	Finish(F.Tasks, std::move(AttachedWork));
	Check(EqualInspectionValue(Gui->Snapshot(), Shared->Snapshot()));
	Check(EqualInspectionValue(Standalone.ReadField(State.Document, RecordType<FModelAsset>().Id, "materialSlots"),
	                           Gui->Get("materialSlots")));
	Check(Gui->Undo() && Shared->Undo() && EqualInspectionValue(Gui->Snapshot(), Before));
	const auto Undone = Standalone.Undo({Changed.Document, Changed.Generation});
	Check(!Undone.bDirty && !Undone.bCanUndo && !Shared->CanUndo());
	const auto Generation = Shared->Generation();
	try
	{
		Attached.SetField("gui-document", Generation, RecordType<FModelAsset>().Id, "materialSlots",
		                  WriteValue(std::vector<FAssetRef>{}));
		Check(false);
	}
	catch (const std::invalid_argument&)
	{
	}
	Check(!Shared->IsEditing() && Shared->Generation() == Generation &&
	      EqualInspectionValue(Shared->Snapshot(), Before));
	FOperationCatalog Catalog;
	RegisterAssetOperations(Catalog, &Attached);
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	const FArchiveNode Arguments(FArchiveNode::FObject{
	    {"document", WriteValue(std::string("gui-document"))},
	    {"generation", WriteValue(std::to_string(Generation))},
	    {"value", FArchiveNode(FArchiveNode::FArray{WriteRecordWire(RecordType<FAssetRef>(), &F.Broken)})}});
	const auto Failed = Outcome(Session, F.Tasks, Session.Call("model.material_slots.set", Arguments));
	Check(Text(Failed, "status") == "failed" && Text(Field(Failed, "error"), "code") == "invalid_arguments");
	Check(Session.PendingCount() == 0);
	Check(!Shared->IsEditing() && Shared->Generation() == Generation &&
	      EqualInspectionValue(Shared->Snapshot(), Before));
	const auto Retry = Finish(
	    F.Tasks, Attached.SetField("gui-document", Generation, RecordType<FModelAsset>().Id, "materialSlots", Value));
	Check(Retry.bDirty && !Retry.bEditing && Shared->CanUndo());
	Check(Shared->Undo() && EqualInspectionValue(Shared->Snapshot(), Before));
	Check(Shared->Redo() && EqualInspectionValue(Shared->Get("materialSlots"), Value));
	Check(F.Files->Read(Gui->Loaded().Path, 1024 * 1024) == Disk);
}

void CheckPreparationFailureAndDrain()
{
	FWorkflowFixture F;
	auto Document = F.Document("workflow-texture.hasset");
	const auto Before = Document->Snapshot();
	const auto Generation = Document->Generation();
	auto Work = FAssetEditWorkflow::Encoding(F.Tasks, Document, Generation, static_cast<EMaterialTextureEncoding>(99));
	try
	{
		Wait(F.Tasks,
		     [&]
		     {
			     return Work->Poll(Document);
		     });
		Check(false);
	}
	catch (const std::invalid_argument&)
	{
	}
	Check(!Document->IsEditing() && Document->Generation() == Generation && !Document->CanUndo());
	Check(EqualInspectionValue(Document->Snapshot(), Before));
	Work = FAssetEditWorkflow::Encoding(F.Tasks, Document, Generation, EMaterialTextureEncoding::Srgb);
	Work->Drain();
	Check(!Work->IsPending() && !Document->IsEditing() && !Document->IsDirty());
	Check(EqualInspectionValue(Document->Snapshot(), Before));
	FSharedWorkspace Workspace(Document);
	FAssetAutomation Attached(F.Assets, F.Tasks, nullptr, &Workspace);
	auto Pending = Attached.SetEncoding({"gui-document", Generation, EMaterialTextureEncoding::Srgb});
	Attached.Drain();
	Check(!Document->IsEditing() && !Document->CanUndo() && EqualInspectionValue(Document->Snapshot(), Before));
	Attached.Close({"gui-document", Generation, true});
	Check(Workspace.Closed == 1);
}

void CheckAttachedRetirementAndSession()
{
	FWorkflowFixture F;
	auto Old = F.Document("workflow-texture.hasset");
	const auto Before = Old->Snapshot();
	FSharedWorkspace Workspace(Old);
	FAssetAutomation Attached(F.Assets, F.Tasks, nullptr, &Workspace);
	auto Pending = Attached.SetEncoding({"gui-document", Old->Generation(), EMaterialTextureEncoding::Srgb});
	auto Replacement = F.Document("workflow-texture.hasset");
	Workspace.Replace(Replacement);
	try
	{
		Finish(F.Tasks, std::move(Pending));
		Check(false);
	}
	catch (const FAutomationError& Error)
	{
		Check(Error.Code == "stale_document");
	}
	Check(!Old->IsEditing() && !Old->IsDirty() && !Replacement->IsDirty());
	Check(EqualInspectionValue(Old->Snapshot(), Before) && EqualInspectionValue(Replacement->Snapshot(), Before));
	FOperationCatalog Catalog;
	RegisterAssetOperations(Catalog, &Attached);
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	const FAssetEncodingRequest Request{"gui-document", Replacement->Generation(), EMaterialTextureEncoding::Srgb};
	const auto Result =
	    Session.Call("texture.set_encoding", WriteRecordWire(RecordType<FAssetEncodingRequest>(), &Request));
	Check(Text(Result, "status") == "running" && Replacement->IsEditing());
	Session.StopAdmission();
	const auto Completed = Outcome(Session, F.Tasks, Result);
	Check(Text(Completed, "status") == "completed" && Session.PendingCount() == 0);
	Check(Replacement->IsDirty() && Replacement->CanUndo() && !Replacement->IsEditing());
	Attached.Drain();
	Check(F.Assets.LoadAsync<FTextureAsset>("workflow-texture.hasset").Get(F.Tasks)->Encoding ==
	      EMaterialTextureEncoding::Linear);
}

void CheckPluginPendingEditShutdown()
{
	FApplicationHost Host(2, 1);
	auto Files = std::make_shared<FMemoryFileSystem>();
	FIOService IO(Host.GetTasks(), Files);
	FAssetService Assets(IO);
	Assets.Types().Register<FTextureAsset>();
	const auto Texture = BuildTextureAsset("Shutdown", EMaterialTextureEncoding::Linear, {1, 1, {8, 16, 32, 255}});
	const auto Encoded = EncodeAsset(RecordType<FTextureAsset>(), &Texture);
	const auto Path = Assets.NormalizePath("pending-shutdown.hasset");
	Files->WriteAtomic(Path, Encoded.Bytes);
	auto Document = std::make_shared<FAssetEditDocument>(Assets.LoadAsync(Path).Get(Host.GetTasks()));
	FSharedWorkspace Workspace(Document);
	Host.GetServices().AddExternal(Assets);
	Host.GetServices().AddExternal<IAssetWorkspace>(Workspace);
	FPluginRegistry Registry;
	RegisterAutomationServices(Registry);
	RegisterAssetAutomation(Registry);
	Host.Start(Registry, {{"automation-session", "automation-assets"}});
	auto& Session = Host.GetServices().Require<FAutomationSession>();
	const FAssetEncodingRequest Request{"gui-document", Document->Generation(), EMaterialTextureEncoding::Srgb};
	const auto Pending =
	    Session.Call("texture.set_encoding", WriteRecordWire(RecordType<FAssetEncodingRequest>(), &Request));
	Check(Text(Pending, "status") == "running" && Document->IsEditing());
	Host.Stop();
	Host.GetServices().Require<FApplicationControl>().RethrowFailure();
	Check(!Document->IsEditing() && Document->IsDirty() && Document->CanUndo());
	Check(Files->Read(Path, 1024 * 1024) == Encoded.Bytes);
	Check(Document->Undo() && !Document->IsDirty());
	Assets.Drain();
}
} // namespace

void CheckAssetWorkflowAdapters()
{
	CheckEncodingParity();
	CheckReferenceParityAndFailure();
	CheckPreparationFailureAndDrain();
	CheckAttachedRetirementAndSession();
	CheckPluginPendingEditShutdown();
}
