#include "AssetOperations.h"
#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/Automation/Session.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <bit>
#include <chrono>
#include <fstream>
#include <source_location>
#include <thread>

namespace Hyperion
{
struct FAssetAutomationTestAccess
{
	static FArchiveNode Snapshot(FAssetAutomation& InProvider, std::string_view InDocument)
	{
		return InProvider.Find(InDocument)->Document->Snapshot();
	}

	static std::uint64_t PreviewGeneration(FAssetAutomation& InProvider, std::string_view InDocument)
	{
		return InProvider.Find(InDocument)->Document->PreviewGeneration();
	}
};
} // namespace Hyperion

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

template<class TFunction>
void Wait(FTaskSystem& InTasks, TFunction InFunction, std::string_view InContext = "condition")
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (!InFunction())
	{
		if (std::chrono::steady_clock::now() >= Deadline)
		{
			throw std::runtime_error("Asset workflow wait timed out: " + std::string(InContext));
		}
		InTasks.PumpMain();
		std::this_thread::yield();
	}
}

template<class T>
T Finish(FTaskSystem& InTasks, TPendingOperation<T> InOperation, std::string_view InContext = "pending operation")
{
	std::optional<T> Result;
	Wait(
	    InTasks,
	    [&]
	    {
		    Result = InOperation.Poll();
		    return Result.has_value();
	    },
	    InContext);
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

	void PumpDocument(std::string_view InId) override
	{
		if (Entry && Entry->Id == InId && Entry->Document)
		{
			Entry->Document->PollSave();
		}
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
	FAssetRef CubeReference;

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
		Material.Parameters.clear();
		Parameter.Name = "Roughness";
		Parameter.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float);
		Parameter.Semantic = "Pbr.RoughnessFactor";
		Parameter.Default = FMaterialAssetValue{Parameter.Type, {std::bit_cast<std::uint32_t>(1.f)}};
		Material.Parameters.push_back(Parameter);
		Material.Values = {{Parameter.Name, *Parameter.Default}};
		Store("workflow-numeric.hasset", Material);
		Parameter.Name = "Albedo";
		Parameter.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
		Parameter.Semantic.clear();
		Parameter.Default = FMaterialAssetValue{Parameter.Type};
		Parameter.Default->Texture = TextureRef;
		Material.Parameters = {Parameter};
		Material.Values = {{Parameter.Name, *Parameter.Default}};
		Store("workflow-reference.hasset", Material);
		FTextureAsset Cube;
		Cube.Name = "Cube";
		Cube.Dimension = ETextureDimension::Cube;
		Cube.Mips = {{1, 1, std::vector<std::uint8_t>(24, 255)}};
		CubeReference = Store("workflow-cube.hasset", Cube);
	}

	std::shared_ptr<FAssetEditDocument> Document(const char* InPath)
	{
		return std::make_shared<FAssetEditDocument>(Assets.LoadAsync(InPath).Get(Tasks));
	}
};

struct FBehaviorState
{
	FArchiveNode Snapshot;
	std::uint64_t Generation{};
	std::uint64_t Preview{};
	bool bDirty{};
	bool bCanUndo{};
	bool bCanRedo{};
};

FBehaviorState Observe(const FAssetEditDocument& InDocument)
{
	return {InDocument.Snapshot(), InDocument.Generation(), InDocument.PreviewGeneration(),
	        InDocument.IsDirty(),  InDocument.CanUndo(),    InDocument.CanRedo()};
}

FBehaviorState Observe(FAssetAutomation& InProvider, const std::string& InDocument)
{
	const auto Info = InProvider.Info({InDocument});
	Check(!Info.bEditing && !Info.bSaving);
	return {FAssetAutomationTestAccess::Snapshot(InProvider, InDocument),
	        Info.Generation,
	        FAssetAutomationTestAccess::PreviewGeneration(InProvider, InDocument),
	        Info.bDirty,
	        Info.bCanUndo,
	        Info.bCanRedo};
}

template<class T> FArchiveNode WireArray(const std::vector<T>& InValues)
{
	FArchiveNode::FArray Result;
	for (const auto& Value : InValues)
	{
		Result.push_back(WriteRecordWire(RecordType<T>(), &Value));
	}
	return FArchiveNode(std::move(Result));
}

struct FBehaviorFixture
{
	FWorkflowFixture Fixture;
	std::shared_ptr<FAssetEditDocument> Gui;
	std::shared_ptr<FAssetEditDocument> Shared;
	FSharedWorkspace Workspace;
	FAssetAutomation Attached;
	FAssetAutomation Standalone;
	FOperationCatalog AttachedCatalog;
	FOperationCatalog StandaloneCatalog;
	std::unique_ptr<FAutomationSession> AttachedSession;
	std::unique_ptr<FAutomationSession> StandaloneSession;
	std::string StandaloneId;
	FBehaviorState Initial;

	explicit FBehaviorFixture(const char* InPath)
	    : Gui(Fixture.Document(InPath)), Shared(Fixture.Document(InPath)), Workspace(Shared),
	      Attached(Fixture.Assets, Fixture.Tasks, nullptr, &Workspace), Standalone(Fixture.Assets, Fixture.Tasks)
	{
		RegisterAssetOperations(AttachedCatalog, &Attached);
		RegisterAssetOperations(StandaloneCatalog, &Standalone);
		AttachedCatalog.Seal();
		StandaloneCatalog.Seal();
		AttachedSession = std::make_unique<FAutomationSession>(AttachedCatalog);
		StandaloneSession = std::make_unique<FAutomationSession>(StandaloneCatalog);
		StandaloneId = Finish(Fixture.Tasks, Standalone.Open({InPath})).Document;
		Initial = Observe(*Gui);
	}

	void Invoke(const char* InOperation, const FArchiveNode::FObject& InFields = {})
	{
		const auto Call = [&](FAssetAutomation& InProvider, FAutomationSession& InSession, const std::string& InId,
		                      const char* InCaller)
		{
			const auto Info = InProvider.Info({InId});
			auto Fields = InFields;
			Fields.emplace("document", WriteValue(InId));
			Fields.emplace("generation", WriteValue(std::to_string(Info.Generation)));
			const auto Result = Outcome(InSession, Fixture.Tasks, InSession.Call(InOperation, FArchiveNode(Fields)));
			if (Text(Result, "status") != "completed")
			{
				throw std::runtime_error("Asset behavior operation failed; operation=" + std::string(InOperation) +
				                         "; caller=" + InCaller + "; path=" + Info.Path + "; request=" +
				                         WriteJson(FArchiveNode(Fields)) + "; result=" + WriteJson(Result));
			}
		};
		Call(Attached, *AttachedSession, "gui-document", "attached");
		Call(Standalone, *StandaloneSession, StandaloneId, "standalone");
	}

	void CheckStates(FArchiveNode::FObject& OutReport, const char* InCase, std::uint64_t InGeneration,
	                 std::array<std::uint64_t, 3> InPreview, bool bInDirty, bool bInUndo, bool bInRedo)
	{
		const std::array States{Observe(*Gui), Observe(Attached, "gui-document"), Observe(Standalone, StandaloneId)};
		const std::array Names{"gui", "attached", "standalone"};
		FArchiveNode::FObject Paths;
		for (std::size_t Index = 0; Index < States.size(); ++Index)
		{
			const auto& State = States[Index];
			Check(EqualInspectionValue(State.Snapshot, States.front().Snapshot));
			Check(State.Generation == Initial.Generation + InGeneration &&
			      State.Preview == Initial.Preview + InPreview[Index]);
			Check(State.bDirty == bInDirty && State.bCanUndo == bInUndo && State.bCanRedo == bInRedo);
			Paths.emplace(Names[Index],
			              FArchiveNode(FArchiveNode::FObject{
			                  {"generationDelta", WriteValue(State.Generation - Initial.Generation)},
			                  {"previewDelta", WriteValue(State.Preview - Initial.Preview)},
			                  {"dirty", WriteValue(State.bDirty)},
			                  {"canUndo", WriteValue(State.bCanUndo)},
			                  {"canRedo", WriteValue(State.bCanRedo)},
			                  {"sameAsInitial", WriteValue(EqualInspectionValue(State.Snapshot, Initial.Snapshot))}}));
		}
		Check(OutReport.emplace(InCase, FArchiveNode(std::move(Paths))).second);
	}

	void RejectOperation(const char* InOperation, const FArchiveNode::FObject& InFields,
	                     std::string_view InExpectedCode)
	{
		const auto Call = [&](FAssetAutomation& InProvider, FAutomationSession& InSession, const std::string& InId)
		{
			auto Fields = InFields;
			Fields.emplace("document", WriteValue(InId));
			Fields.emplace("generation", WriteValue(std::to_string(InProvider.Info({InId}).Generation)));
			const auto Result = Outcome(InSession, Fixture.Tasks, InSession.Call(InOperation, FArchiveNode(Fields)));
			Check(Text(Result, "status") == "failed" && Text(Field(Result, "error"), "code") == InExpectedCode);
		};
		Call(Attached, *AttachedSession, "gui-document");
		Call(Standalone, *StandaloneSession, StandaloneId);
	}
};

void CheckNodeEffectBaseline(FArchiveNode::FObject& OutReport)
{
	FBehaviorFixture F("workflow-model.hasset");
	auto Nodes = ReadValue<std::vector<FModelNode>>(F.Gui->Get("nodes"));
	Nodes.front().Name = "Metadata only";
	CommitAssetField(*F.Gui, "nodes", WriteValue(Nodes));
	F.Invoke("model.nodes.set", {{"value", WireArray(Nodes)}});
	F.CheckStates(OutReport, "nodeName", 1, {0, 0, 0}, true, true, false);
	Check(F.Gui->Undo());
	F.Invoke("asset.undo");
	F.CheckStates(OutReport, "nodeNameUndo", 2, {0, 0, 0}, false, false, true);
	Check(EqualInspectionValue(F.Gui->Snapshot(), F.Initial.Snapshot));
	Check(F.Gui->Redo());
	F.Invoke("asset.redo");
	F.CheckStates(OutReport, "nodeNameRedo", 3, {0, 0, 0}, true, true, false);
	Nodes.front().Name = "Moved and renamed";
	Nodes.front().Local = Translation({3, 4, 5});
	CommitAssetField(*F.Gui, "nodes", WriteValue(Nodes));
	F.Invoke("model.nodes.set", {{"value", WireArray(Nodes)}});
	F.CheckStates(OutReport, "nodeNameAndLocal", 4, {1, 1, 1}, true, true, false);
	CommitAssetField(*F.Gui, "nodes", WriteValue(Nodes));
	F.Invoke("model.nodes.set", {{"value", WireArray(Nodes)}});
	F.CheckStates(OutReport, "nodeEqual", 5, {1, 1, 1}, true, true, false);
	Check(F.Gui->Undo());
	F.Invoke("asset.undo");
	F.CheckStates(OutReport, "nodeEqualUndo", 6, {1, 1, 1}, true, true, true);
	Check(ReadValue<std::vector<FModelNode>>(F.Gui->Get("nodes")).front().Local.Values ==
	      Translation({3, 4, 5}).Values);
}

void CheckPrimitiveEffectBaseline(FArchiveNode::FObject& OutReport)
{
	FBehaviorFixture F("workflow-model.hasset");
	auto Values = DescribeModelPrimitives(*F.Gui);
	Values.front().Name = "Primitive metadata";
	SetModelPrimitives(*F.Gui, Values);
	F.Invoke("model.primitives.set", {{"values", WireArray(Values)}});
	F.CheckStates(OutReport, "primitiveName", 1, {0, 0, 0}, true, true, false);
	Values.front().Name = "Unassigned primitive";
	Values.front().Material = -1;
	SetModelPrimitives(*F.Gui, Values);
	F.Invoke("model.primitives.set", {{"values", WireArray(Values)}});
	F.CheckStates(OutReport, "primitiveNameAndMaterial", 2, {1, 1, 1}, true, true, false);
	SetModelPrimitives(*F.Gui, Values);
	F.Invoke("model.primitives.set", {{"values", WireArray(Values)}});
	F.CheckStates(OutReport, "primitiveEqual", 3, {1, 1, 1}, true, true, false);
	const auto Actual = ReadValue<std::vector<FModelPrimitive>>(F.Gui->Get("primitives"));
	const auto Initial = ReadValue<FModelAsset>(F.Initial.Snapshot);
	Check(Actual.front().Name == "Unassigned primitive" && Actual.front().Material == -1);
	Check(Actual.front().Positions == Initial.Primitives.front().Positions &&
	      Actual.front().Indices == Initial.Primitives.front().Indices &&
	      Actual.front().Id == Initial.Primitives.front().Id);
}

void CheckNormalizedEffectBaseline(FArchiveNode::FObject& OutReport)
{
	FBehaviorFixture F("workflow-numeric.hasset");
	auto Values = ReadValue<FMaterialAssetValues>(F.Gui->Get("values"));
	Values.front().Value.Words.front() = std::bit_cast<std::uint32_t>(2.f);
	CommitAssetField(*F.Gui, "values", WriteValue(Values));
	F.Invoke("material.values.set", {{"value", WireArray(Values)}});
	F.CheckStates(OutReport, "materialNormalizedEqual", 1, {0, 0, 0}, true, true, false);
	Check(EqualInspectionValue(F.Gui->Snapshot(), F.Initial.Snapshot));
	const std::vector<FMaterialNumericEdit> Edits{{"Roughness", {2.0}}};
	const auto Numeric = PrepareMaterialNumeric(ReadValue<FMaterialAsset>(F.Gui->Snapshot()), Edits);
	CommitAssetField(*F.Gui, "values", WriteValue(Numeric));
	F.Invoke("material.numeric.set", {{"edits", WireArray(Edits)}});
	F.CheckStates(OutReport, "materialNumericNormalizedEqual", 2, {0, 0, 0}, true, true, false);
	Check(EqualInspectionValue(F.Gui->Snapshot(), F.Initial.Snapshot));
	Check(F.Gui->Undo());
	F.Invoke("asset.undo");
	F.CheckStates(OutReport, "materialNumericEqualUndo", 3, {0, 0, 0}, true, true, true);
	Check(F.Gui->Undo());
	F.Invoke("asset.undo");
	F.CheckStates(OutReport, "materialNormalizedEqualUndo", 4, {0, 0, 0}, false, false, true);
}

void CheckNameAndEncodingBaseline(FArchiveNode::FObject& OutReport)
{
	FBehaviorFixture F("workflow-texture.hasset");
	CommitAssetField(*F.Gui, "name", F.Gui->Get("name"));
	F.Invoke("asset.rename", {{"name", F.Gui->Get("name")}});
	F.CheckStates(OutReport, "assetNameEqual", 1, {0, 0, 0}, true, true, false);
	Check(EqualInspectionValue(F.Gui->Snapshot(), F.Initial.Snapshot));
	auto Work =
	    FAssetEditWorkflow::Encoding(F.Fixture.Tasks, F.Gui, F.Gui->Generation(), EMaterialTextureEncoding::Linear);
	Wait(F.Fixture.Tasks,
	     [&]
	     {
		     return Work->Poll(F.Gui);
	     });
	F.Invoke("texture.set_encoding", {{"encoding", WriteValue(EMaterialTextureEncoding::Linear)}});
	F.CheckStates(OutReport, "encodingEqual", 2, {0, 0, 0}, true, true, false);
	Check(EqualInspectionValue(F.Gui->Snapshot(), F.Initial.Snapshot));
}

void CheckFieldPersistenceBaseline()
{
	for (int Path = 0; Path < 3; ++Path)
	{
		FBehaviorFixture F("workflow-model.hasset");
		auto Nodes = ReadValue<std::vector<FModelNode>>(F.Gui->Get("nodes"));
		Nodes.front().Name = "Persisted edit";
		Nodes.front().Local = Translation({3, 4, 5});
		CommitAssetField(*F.Gui, "nodes", WriteValue(Nodes));
		F.Invoke("model.nodes.set", {{"value", WireArray(Nodes)}});
		if (Path == 0)
		{
			F.Gui->Save(F.Fixture.Assets);
			Wait(
			    F.Fixture.Tasks,
			    [&]
			    {
				    F.Gui->PollSave();
				    return !F.Gui->IsSaving();
			    },
			    "GUI persistence save");
			Check(!F.Gui->IsDirty() && F.Gui->Error.empty());
		}
		else
		{
			auto& Provider = Path == 1 ? F.Attached : F.Standalone;
			const std::string Document = Path == 1 ? "gui-document" : F.StandaloneId;
			const auto Saved = Finish(F.Fixture.Tasks, Provider.Save({Document, Provider.Info({Document}).Generation}),
			                          Path == 1 ? "attached persistence save" : "standalone persistence save");
			Check(!Saved.bDirty && !Saved.bSaving && Saved.Error.empty());
		}
		const auto Saved = F.Fixture.Assets.LoadAsync<FModelAsset>("workflow-model.hasset").Get(F.Fixture.Tasks);
		Check(EqualInspectionValue(WriteValue(*Saved), F.Gui->Snapshot()));
		Check(Saved->Nodes.front().Name == "Persisted edit" &&
		      Saved->Nodes.front().Local.Values == Translation({3, 4, 5}).Values);
	}
}

void CheckFieldEffectBaseline()
{
	FArchiveNode::FObject Report;
	CheckNodeEffectBaseline(Report);
	CheckPrimitiveEffectBaseline(Report);
	CheckNormalizedEffectBaseline(Report);
	CheckNameAndEncodingBaseline(Report);
	CheckFieldPersistenceBaseline();
	std::ofstream File("asset-field-policy-behavior.json", std::ios::binary);
	File << WriteJson(FArchiveNode(std::move(Report)));
	Check(File.good());
}

void CheckReferencePolicyFailures()
{
	FBehaviorFixture F("workflow-reference.hasset");

	struct FFailureCase
	{
		FAssetRef Reference;
		std::string_view Code;
	};

	const std::array Cases{
	    FFailureCase{F.Fixture.CubeReference, "invalid_arguments"},
	    FFailureCase{F.Fixture.Replacement, "invalid_arguments"},
	    FFailureCase{{"", "missing-texture.hasset", RecordType<FTextureAsset>().Id, {}}, "operation_failed"}};
	for (const auto& Case : Cases)
	{
		auto Values = ReadValue<FMaterialAssetValues>(F.Gui->Get("values"));
		auto WireValues = WireArray(Values);
		auto& WireEntry =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FArray>(WireValues.Value).front().Value);
		std::get<FArchiveNode::FObject>(WireEntry.at("value").Value).at("texture") =
		    WriteRecordWire(RecordType<FAssetRef>(), &Case.Reference);
		Values.front().Value.Texture = Case.Reference;
		bool bRejected{};
		try
		{
			const auto Work = FAssetEditWorkflow::Field(
			    F.Fixture.Tasks, F.Fixture.Assets, F.Gui, F.Gui->Generation(),
			    ResolveAssetFieldPolicy(*F.Gui->Loaded().Type, &FMaterialAsset::Values).Field(), WriteValue(Values));
			Wait(F.Fixture.Tasks,
			     [&]
			     {
				     return Work->Poll(F.Gui);
			     });
		}
		catch (const std::invalid_argument&)
		{
			Check(Case.Code == "invalid_arguments");
			bRejected = true;
		}
		catch (const std::runtime_error& Error)
		{
			Check(Case.Code == "operation_failed" &&
			      std::string_view(Error.what()).find("missing-texture.hasset") != std::string_view::npos);
			bRejected = true;
		}
		Check(bRejected);
		F.RejectOperation("material.values.set", {{"value", std::move(WireValues)}}, Case.Code);
		FArchiveNode::FObject State;
		F.CheckStates(State, "rejectedReference", 0, {0, 0, 0}, false, false, false);
		Check(EqualInspectionValue(F.Gui->Snapshot(), F.Initial.Snapshot));
	}
}

void CheckReferenceEffects()
{
	FBehaviorFixture F("workflow-reference.hasset");
	const auto Replacement = F.Fixture.Store(
	    "workflow-new-texture.hasset",
	    BuildTextureAsset("Replacement texture", EMaterialTextureEncoding::Linear, {1, 1, {77, 12, 31, 255}}));
	auto Values = ReadValue<FMaterialAssetValues>(F.Gui->Get("values"));
	Values.front().Value.Texture = Replacement;
	const auto Submit = [&]
	{
		const auto Work = FAssetEditWorkflow::Field(
		    F.Fixture.Tasks, F.Fixture.Assets, F.Gui, F.Gui->Generation(),
		    ResolveAssetFieldPolicy(*F.Gui->Loaded().Type, &FMaterialAsset::Values).Field(), WriteValue(Values));
		Wait(F.Fixture.Tasks,
		     [&]
		     {
			     return Work->Poll(F.Gui);
		     });
		F.Invoke("material.values.set", {{"value", WireArray(Values)}});
	};
	FArchiveNode::FObject State;
	Submit();
	F.CheckStates(State, "referenceChanged", 1, {1, 1, 1}, true, true, false);
	Check(ReadValue<FMaterialAssetValues>(F.Gui->Get("values")).front().Value.Texture == Replacement);
	const auto Changed = F.Gui->Snapshot();
	Submit();
	F.CheckStates(State, "referenceEqual", 2, {1, 1, 1}, true, true, false);
	Check(F.Gui->Undo());
	F.Invoke("asset.undo");
	F.CheckStates(State, "referenceEqualUndo", 3, {1, 1, 1}, true, true, true);
	Check(EqualInspectionValue(F.Gui->Snapshot(), Changed));
	Check(F.Gui->Undo());
	F.Invoke("asset.undo");
	F.CheckStates(State, "referenceChangedUndo", 4, {2, 2, 2}, false, false, true);
	Check(EqualInspectionValue(F.Gui->Snapshot(), F.Initial.Snapshot));
	Check(F.Gui->Redo());
	F.Invoke("asset.redo");
	F.CheckStates(State, "referenceChangedRedo", 5, {3, 3, 3}, true, true, true);
	Check(EqualInspectionValue(F.Gui->Snapshot(), Changed));
	Check(F.Gui->Redo());
	F.Invoke("asset.redo");
	F.CheckStates(State, "referenceEqualRedo", 6, {3, 3, 3}, true, true, false);
}

void CheckEqualMaterialSlotEffects()
{
	FBehaviorFixture F("workflow-model.hasset");
	const auto Slots = ReadValue<std::vector<FAssetRef>>(F.Gui->Get("materialSlots"));
	const auto Work = FAssetEditWorkflow::Field(
	    F.Fixture.Tasks, F.Fixture.Assets, F.Gui, F.Gui->Generation(),
	    ResolveAssetFieldPolicy(*F.Gui->Loaded().Type, &FModelAsset::MaterialSlots).Field(), WriteValue(Slots));
	Wait(F.Fixture.Tasks,
	     [&]
	     {
		     return Work->Poll(F.Gui);
	     });
	F.Invoke("model.material_slots.set", {{"value", WireArray(Slots)}});
	FArchiveNode::FObject State;
	F.CheckStates(State, "materialSlotsEqual", 1, {0, 0, 0}, true, true, false);
	Check(EqualInspectionValue(F.Gui->Snapshot(), F.Initial.Snapshot));
	Check(F.Gui->Undo());
	F.Invoke("asset.undo");
	F.CheckStates(State, "materialSlotsEqualUndo", 2, {0, 0, 0}, false, false, true);
	Check(F.Gui->Redo());
	F.Invoke("asset.redo");
	F.CheckStates(State, "materialSlotsEqualRedo", 3, {0, 0, 0}, true, true, false);
}

void CheckDirectReferenceProtection()
{
	FWorkflowFixture F;
	auto Document = F.Document("workflow-model.hasset");
	const auto Initial = Observe(*Document);
	for (const bool bDirectSet : {false, true})
	{
		bool bRejected{};
		try
		{
			if (bDirectSet)
			{
				Document->Set("materialSlots", WriteValue(std::vector{F.Replacement}));
			}
			else
			{
				CommitAssetField(*Document,
				                 ResolveAssetFieldPolicy(*Document->Loaded().Type, &FModelAsset::MaterialSlots).Field(),
				                 WriteValue(std::vector{F.Replacement}));
			}
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		Check(bRejected && !Document->IsDirty() && !Document->CanUndo());
		Check(Document->Generation() == Initial.Generation && Document->PreviewGeneration() == Initial.Preview);
		Check(EqualInspectionValue(Document->Snapshot(), Initial.Snapshot));
	}
}

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
	const auto InitialPreview = Gui->PreviewGeneration();
	const auto CheckPreview = [&](std::uint64_t InDelta)
	{
		Check(Gui->PreviewGeneration() == InitialPreview + InDelta &&
		      AttachedDocument->PreviewGeneration() == InitialPreview + InDelta &&
		      FAssetAutomationTestAccess::PreviewGeneration(Standalone, State.Document) == InitialPreview + InDelta);
	};
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
	CheckPreview(1);
	Check(Shared.Generation == Gui->Generation() && Changed.Generation == Gui->Generation());
	Check(EqualInspectionValue(Gui->Snapshot(), AttachedDocument->Snapshot()));
	Check(EqualInspectionValue(Standalone.ReadField(State.Document, RecordType<FTextureAsset>().Id, "mips"),
	                           Gui->Get("mips")));
	Check(Gui->Undo() && AttachedDocument->Undo());
	const auto Undone = Standalone.Undo({Changed.Document, Changed.Generation});
	Check(!Gui->IsDirty() && !AttachedDocument->IsDirty() && !Undone.bDirty && !Undone.bCanUndo);
	CheckPreview(2);
	Check(EqualInspectionValue(Gui->Snapshot(), Initial) &&
	      EqualInspectionValue(AttachedDocument->Snapshot(), Initial));
	Check(Gui->Redo() && AttachedDocument->Redo());
	const auto Redone = Standalone.Redo({Undone.Document, Undone.Generation});
	Check(Redone.bDirty && !Redone.bCanRedo && Gui->IsDirty() && AttachedDocument->IsDirty());
	CheckPreview(3);
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
	CheckFieldEffectBaseline();
	CheckReferencePolicyFailures();
	CheckReferenceEffects();
	CheckEqualMaterialSlotEffects();
	CheckDirectReferenceProtection();
	CheckEncodingParity();
	CheckReferenceParityAndFailure();
	CheckPreparationFailureAndDrain();
	CheckAttachedRetirementAndSession();
	CheckPluginPendingEditShutdown();
}
