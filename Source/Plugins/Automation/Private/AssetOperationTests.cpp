#include "AssetOperations.h"
#include "ContentRootOperations.h"
#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Content/ContentQueries.h"
#include "Hyperion/Environment/SkyAsset.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <source_location>
#include <thread>

namespace Hyperion
{
void CheckMaterialNumericAutomation(FTaskSystem& InTasks, FAssetService& InAssets, FMemoryFileSystem& InFiles);
}

namespace
{
using namespace Hyperion;

void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Asset automation check failed: " + std::to_string(InLocation.line()));
	}
}

const FArchiveNode& Field(const FArchiveNode& InNode, std::string_view InKey)
{
	return std::get<FArchiveNode::FObject>(InNode.Value).at(std::string(InKey));
}

std::string Text(const FArchiveNode& InNode, std::string_view InKey)
{
	return ReadValue<std::string>(Field(InNode, InKey));
}

FTextureAsset Fixture()
{
	return BuildTextureAsset("Original", EMaterialTextureEncoding::Linear,
	                         {2, 2, {0, 0, 0, 255, 255, 255, 255, 255, 128, 128, 128, 255, 64, 64, 64, 255}});
}

FArchiveNode Wait(FAutomationSession& InSession, FTaskSystem& InTasks, FArchiveNode InResult)
{
	if (Text(InResult, "status") != "running")
	{
		return InResult;
	}
	const auto Job = Text(InResult, "job");
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	do
	{
		Check(std::chrono::steady_clock::now() < Deadline);
		InTasks.PumpMain();
		InResult = InSession.GetJob(Job);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	} while (Text(InResult, "status") == "running");
	return Field(InResult, "outcome");
}

template<class T> FArchiveNode Call(FAutomationSession& InSession, std::string_view InOperation, const T& InRequest)
{
	return InSession.Call(InOperation, WriteRecordWire(RecordType<T>(), &InRequest));
}

FAssetDocumentInfo Info(const FArchiveNode& InResult)
{
	Check(Text(InResult, "status") == "completed");
	return *std::static_pointer_cast<FAssetDocumentInfo>(
	    ReadRecordWire(RecordType<FAssetDocumentInfo>(), Field(InResult, "result")));
}

void Failure(const FArchiveNode& InResult, std::string_view InCode)
{
	Check(Text(InResult, "status") == "failed" && Text(Field(InResult, "error"), "code") == InCode);
}

void CheckPropertySchema(const FArchiveNode& InSchema, std::string_view InType,
                         const std::vector<std::string>& InFields, const std::vector<std::string>& InRequired)
{
	Check(Text(InSchema, "x-hyperion-type") == InType &&
	      ReadValue<unsigned>(Field(InSchema, "x-hyperion-version")) == 1);
	Check(Text(InSchema, "type") == "object" && !ReadValue<bool>(Field(InSchema, "additionalProperties")));
	std::vector<std::string> Names;
	for (const auto& [Name, Value] : std::get<FArchiveNode::FObject>(Field(InSchema, "properties").Value))
	{
		Names.push_back(Name);
	}
	Check(Names == InFields && ReadValue<std::vector<std::string>>(Field(InSchema, "required")) == InRequired);
}

void CheckPropertyQuerySchema(const FArchiveNode& InSchema)
{
	CheckPropertySchema(InSchema, "automation.asset.property.query", {"document", "generation", "limit", "offset"},
	                    {"document", "generation"});
	const auto& Properties = Field(InSchema, "properties");
	Check(Text(Field(Properties, "document"), "type") == "string");
	Check(Text(Field(Properties, "generation"), "type") == "string");
	Check(Text(Field(Properties, "generation"), "format") == "uint64");
	Check(Text(Field(Properties, "generation"), "pattern") == "^(0|[1-9][0-9]*)$");
	for (const auto* Name : {"offset", "limit"})
	{
		const auto& Value = Field(Properties, Name);
		Check(Text(Value, "type") == "integer");
		Check(ReadValue<double>(Field(Value, "minimum")) == 0 &&
		      ReadValue<double>(Field(Value, "maximum")) == 4294967295.0);
	}
	Check(ReadValue<unsigned>(Field(Field(Properties, "offset"), "default")) == 0);
	Check(ReadValue<unsigned>(Field(Field(Properties, "limit"), "default")) == 50);
}

void CheckPropertyContracts()
{
	struct FExpected
	{
		const char* Id;
		bool bWritable;
		const char* ValueKind;
		const char* RecordId;
		unsigned RecordVersion = 1;
	};

	const FExpected Expected[] = {{"model.nodes", true, "array", "hyperion.modelnode", 2},
	                              {"model.material_slots", true, "array", "hyperion.assetref"},
	                              {"material.values", true, "array", "hyperion.materialassetentry"},
	                              {"material.parameters", false, "array", "hyperion.materialassetparameter"},
	                              {"material.passes", false, "array", "hyperion.materialpass", 2},
	                              {"model.roots", false, "array", ""},
	                              {"sky.radiance", false, "object", "hyperion.assetref"},
	                              {"sky.specular", false, "object", "hyperion.assetref"},
	                              {"sky.brdf", false, "object", "hyperion.assetref"},
	                              {"sky.irradiance", false, "array", ""},
	                              {"sky.convention", false, "integer", ""},
	                              {"texture.dimension", false, "integer", ""},
	                              {"texture.format", false, "integer", ""},
	                              {"texture.encoding", false, "integer", ""}};
	FOperationCatalog Catalog;
	RegisterAssetOperations(Catalog, nullptr);
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	FArchiveNode::FObject Operations;
	for (const auto& Item : Expected)
	{
		const std::string Id = Item.Id;
		const auto& Get = Catalog.Find(Id + ".get");
		Check(Get.Info.Version == 1 && Get.Info.Owner == "automation-assets" && Get.Info.bReadOnly &&
		      !Get.bAsynchronous);
		Check(Get.Request->Id == "automation.asset.property.query" && Get.Result->Id == Id + ".value");
		const auto Description = Catalog.Describe(Id + ".get");
		Operations.emplace(Id + ".get", Description);
		CheckPropertyQuerySchema(Field(Description, "inputSchema"));
		const auto& Output = Field(Description, "outputSchema");
		CheckPropertySchema(Output, Id + ".value", {"next", "total", "value"}, {});
		const auto& Properties = Field(Output, "properties");
		Check(Text(Field(Properties, "total"), "format") == "uint64");
		const auto& Next = std::get<FArchiveNode::FArray>(Field(Field(Properties, "next"), "anyOf").Value);
		Check(Next.size() == 2 && Text(Next[0], "type") == "integer" && Text(Next[1], "type") == "null");
		const auto& Value = Field(Properties, "value");
		Check(Text(Value, "type") == Item.ValueKind);
		if (*Item.RecordId)
		{
			const auto& Record = Text(Value, "type") == "array" ? Field(Value, "items") : Value;
			Check(Text(Record, "x-hyperion-type") == Item.RecordId);
			Check(ReadValue<unsigned>(Field(Record, "x-hyperion-version")) == Item.RecordVersion);
		}
		Failure(Session.Call(Id + ".get", Get.Info.Example), "unavailable");
		if (!Item.bWritable)
		{
			Failure(Session.Call(Id + ".set", FArchiveNode(FArchiveNode::FObject{})), "not_found");
			continue;
		}
		const auto& Set = Catalog.Find(Id + ".set");
		Check(Set.Info.Version == 1 && !Set.Info.bReadOnly && Set.bAsynchronous);
		Check(Set.Request->Id == Id + ".request" && Set.Result->Id == "automation.asset.info" &&
		      Set.Result->Version == 1);
		const auto SetDescription = Catalog.Describe(Id + ".set");
		Operations.emplace(Id + ".set", SetDescription);
		const auto& Input = Field(SetDescription, "inputSchema");
		CheckPropertySchema(Input, Id + ".request", {"document", "generation", "offset", "value"},
		                    {"document", "generation", "value"});
		Check(Text(Field(Field(Input, "properties"), "value"), "type") == "array");
		Check(Text(Field(Field(Input, "properties"), "generation"), "format") == "uint64");
		Failure(Session.Call(Id + ".set", Set.Info.Example), "unavailable");
	}
	Check(Operations.size() == 17);
	std::ofstream File("asset-property-api-contract.json", std::ios::binary);
	File << WriteJson(FArchiveNode(std::move(Operations)));
	Check(File.good());
}

FArchiveNode PropertyQuery(const FAssetDocumentInfo& InDocument, unsigned InOffset = 0, unsigned InLimit = 50)
{
	return FArchiveNode(FArchiveNode::FObject{{"document", WriteValue(InDocument.Document)},
	                                          {"generation", WriteValue(std::to_string(InDocument.Generation))},
	                                          {"offset", WriteValue(InOffset)},
	                                          {"limit", WriteValue(InLimit)}});
}

template<class T>
void StorePropertyFixture(FAssetService& InAssets, FMemoryFileSystem& InFiles, const char* InPath, const T& InAsset)
{
	InFiles.WriteAtomic(InAssets.NormalizePath(InPath), EncodeAsset(RecordType<T>(), &InAsset).Bytes);
}

void CheckDistinctSkyProperties(FAssetService& InAssets, FTaskSystem& InTasks, FMemoryFileSystem& InFiles)
{
	FSkyAsset Sky;
	Sky.Name = "Distinct property references";
	Sky.Radiance = {"", "property-radiance.hasset", "hyperion.textureasset", ""};
	Sky.Specular = {"", "property-specular.hasset", "hyperion.textureasset", ""};
	Sky.Brdf = {"", "property-brdf.hasset", "hyperion.textureasset", ""};
	Sky.Irradiance[0] = {1.25f, 2.5f, 3.75f};
	StorePropertyFixture(InAssets, InFiles, "property-sky.hasset", Sky);
	InAssets.Types().Register<FSkyAsset>();
	FAssetAutomation Provider(InAssets, InTasks);
	FOperationCatalog Catalog;
	RegisterAssetOperations(Catalog, &Provider);
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	const auto Document =
	    Info(Wait(Session, InTasks, Call(Session, "asset.open", FAssetOpenRequest{"property-sky.hasset"})));
	for (const auto& [Operation, Path] :
	     {std::pair{"sky.radiance.get", "property-radiance.hasset"},
	      std::pair{"sky.specular.get", "property-specular.hasset"}, std::pair{"sky.brdf.get", "property-brdf.hasset"}})
	{
		const auto Result = Session.Call(Operation, PropertyQuery(Document));
		Check(Text(Result, "status") == "completed");
		const auto& Value = Field(Field(Result, "result"), "value");
		Check(Text(Value, "path") == Path && Text(Value, "type") == "hyperion.textureasset");
		Check(Text(Value, "id").empty() && Text(Value, "revision").empty());
		Check(Text(Field(Result, "result"), "total") == "0");
	}
	const auto Irradiance = Session.Call("sky.irradiance.get", PropertyQuery(Document));
	Check(Text(Irradiance, "status") == "completed");
	const auto& Coefficients = std::get<FArchiveNode::FArray>(Field(Field(Irradiance, "result"), "value").Value);
	Check(Coefficients.size() == 9);
	const auto& FirstCoefficient = std::get<FArchiveNode::FArray>(Coefficients[0].Value);
	Check(FirstCoefficient.size() == 3 && ReadValue<float>(FirstCoefficient[0]) == 1.25f &&
	      ReadValue<float>(FirstCoefficient[1]) == 2.5f && ReadValue<float>(FirstCoefficient[2]) == 3.75f);
	const auto Convention = Session.Call("sky.convention.get", PropertyQuery(Document));
	Check(ReadValue<unsigned>(Field(Field(Convention, "result"), "value")) == 1);
	Failure(Session.Call("sky.radiance.get", PropertyQuery(Document, 0, 0)), "invalid_arguments");
	Failure(Session.Call("sky.radiance.get", PropertyQuery(Document, 0, 101)), "invalid_arguments");
	Provider.Drain();
}

FModelAsset PropertyModel()
{
	FModelAsset Model;
	Model.Name = "Property model";
	Model.MaterialSlots.push_back({"", "property-material.hasset", "hyperion.materialasset", ""});
	FModelPrimitive Primitive;
	Primitive.Positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
	Primitive.Indices = {0, 1, 2};
	Primitive.Material = 0;
	Model.Primitives.push_back(Primitive);
	Model.Nodes = {{"First", Identity(), {0}, {}, "first"}, {"Second", Identity(), {0}, {}, "second"}};
	Model.Roots = {0, 1};
	AssignModelSubresourceIds(Model);
	return Model;
}

void CheckPropertyRangeEditing(FAssetService& InAssets, FTaskSystem& InTasks, FMemoryFileSystem& InFiles)
{
	const auto Model = PropertyModel();
	StorePropertyFixture(InAssets, InFiles, "property-model.hasset", Model);
	InAssets.Types().Register<FModelAsset>();
	FAssetAutomation Provider(InAssets, InTasks);
	FOperationCatalog Catalog;
	RegisterAssetOperations(Catalog, &Provider);
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	auto Document =
	    Info(Wait(Session, InTasks, Call(Session, "asset.open", FAssetOpenRequest{"property-model.hasset"})));
	const auto InitialGeneration = Document.Generation;
	const auto First = Session.Call("model.nodes.get", PropertyQuery(Document, 0, 1));
	const auto& FirstPage = Field(First, "result");
	Check(Text(FirstPage, "total") == "2" && ReadValue<unsigned>(Field(FirstPage, "next")) == 1);
	Check(Text(std::get<FArchiveNode::FArray>(Field(FirstPage, "value").Value).at(0), "name") == "First");
	const auto Second = Session.Call("model.nodes.get", PropertyQuery(Document, 1, 1));
	auto Replacement = Field(Field(Second, "result"), "value");
	auto& Node = std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FArray>(Replacement.Value).at(0).Value);
	Check(Text(std::get<FArchiveNode::FArray>(Replacement.Value)[0], "id") == "second");
	Node.at("name") = WriteValue(std::string("Edited second"));
	FArchiveNode Request(FArchiveNode::FObject{{"document", WriteValue(Document.Document)},
	                                           {"generation", WriteValue(std::to_string(Document.Generation))},
	                                           {"value", Replacement},
	                                           {"offset", WriteValue(1u)}});
	Document = Info(Wait(Session, InTasks, Session.Call("model.nodes.set", Request)));
	Check(Document.bDirty && Document.bCanUndo && Document.Generation == InitialGeneration + 1);
	const auto Edited =
	    ReadValue<std::vector<FModelNode>>(Provider.ReadField(Document.Document, "hyperion.modelasset", "nodes"));
	Check(Edited.size() == 2 && Edited[0].Name == "First" && Edited[1].Name == "Edited second");
	Check(Edited[0].Id == "first" && Edited[1].Id == "second");
	Failure(Session.Call("model.nodes.set", Request), "stale_revision");
	auto Stale = Document;
	Stale.Generation = InitialGeneration;
	Failure(Session.Call("model.nodes.get", PropertyQuery(Stale)), "stale_revision");
	std::get<FArchiveNode::FObject>(Request.Value).at("generation") = WriteValue(std::to_string(Document.Generation));
	std::get<FArchiveNode::FObject>(Request.Value).at("offset") = WriteValue(2u);
	Failure(Session.Call("model.nodes.set", Request), "invalid_arguments");
	Check(Provider.Info({Document.Document}).Generation == Document.Generation);
	Document = Info(Call(Session, "asset.undo", FAssetMutationRequest{Document.Document, Document.Generation}));
	Check(!Document.bDirty && !Document.bCanUndo);
	Check(ReadValue<std::vector<FModelNode>>(Provider.ReadField(Document.Document, "hyperion.modelasset", "nodes"))[1]
	          .Name == "Second");
	Document = Info(Call(Session, "asset.redo", FAssetMutationRequest{Document.Document, Document.Generation}));
	Document = Info(Wait(Session, InTasks,
	                     Call(Session, "asset.save", FAssetMutationRequest{Document.Document, Document.Generation})));
	Check(!Document.bDirty &&
	      InAssets.LoadAsync<FModelAsset>("property-model.hasset").Get(InTasks)->Nodes[1].Name == "Edited second");
	Provider.Drain();
}

void CheckWorkflowCompletion(FAssetService& InAssets, FTaskSystem& InTasks)
{
	InAssets.Invalidate("automation-texture.hasset");
	const auto Loaded = InAssets.LoadAsync("automation-texture.hasset").Get(InTasks);
	auto Document = std::make_shared<FAssetEditDocument>(Loaded);
	const auto Before = Document->Snapshot();
	auto Work = FAssetEditWorkflow::Encoding(InTasks, Document, Document->Generation(), EMaterialTextureEncoding::Srgb);
	Check(Document->IsEditing());
	try
	{
		FAssetEditWorkflow::Encoding(InTasks, Document, Document->Generation(), EMaterialTextureEncoding::Srgb);
		Check(false);
	}
	catch (const FAssetWorkflowError& Error)
	{
		Check(Error.Code == AssetWorkflowErrors::Busy);
	}
	while (!Work->Poll(Document))
	{
		InTasks.PumpMain();
		std::this_thread::yield();
	}
	Check(!Document->IsEditing() && Document->IsDirty() && Document->CanUndo());
	Check(Document->Undo() && EqualInspectionValue(Document->Snapshot(), Before));
	Work = FAssetEditWorkflow::Encoding(InTasks, Document, Document->Generation(), EMaterialTextureEncoding::Srgb);
	Document->Set("name", WriteValue(std::string("Intervening")));
	const auto Changed = Document->Snapshot();
	try
	{
		while (!Work->Poll(Document))
		{
			InTasks.PumpMain();
			std::this_thread::yield();
		}
		Check(false);
	}
	catch (const FAssetWorkflowError& Error)
	{
		Check(Error.Code == AssetWorkflowErrors::StaleRevision);
	}
	Check(!Document->IsEditing() && EqualInspectionValue(Document->Snapshot(), Changed));
	Work = FAssetEditWorkflow::Encoding(InTasks, Document, Document->Generation(), EMaterialTextureEncoding::Linear);
	try
	{
		while (!Work->Poll(nullptr))
		{
			InTasks.PumpMain();
			std::this_thread::yield();
		}
		Check(false);
	}
	catch (const FAssetWorkflowError& Error)
	{
		Check(Error.Code == AssetWorkflowErrors::StaleDocument);
	}
	Check(!Document->IsEditing() && EqualInspectionValue(Document->Snapshot(), Changed));
	Work = FAssetEditWorkflow::Encoding(InTasks, Document, Document->Generation(), EMaterialTextureEncoding::Linear);
	Work->Drain();
	Check(!Document->IsEditing() && EqualInspectionValue(Document->Snapshot(), Changed));
}

void CheckEditing(FAssetService& InAssets, FTaskSystem& InTasks, FMemoryFileSystem& InFiles)
{
	const auto Path = InAssets.NormalizePath("automation-texture.hasset");
	const auto Texture = Fixture();
	InFiles.WriteAtomic(Path, EncodeAsset(RecordType<FTextureAsset>(), &Texture).Bytes);
	InAssets.Types().Register<FTextureAsset>();
	FAssetEditDocument Gui(InAssets.LoadAsync(Path).Get(InTasks));
	FAssetAutomation Provider(InAssets, InTasks);
	FOperationCatalog Catalog;
	RegisterAssetOperations(Catalog, &Provider);
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	auto State = Info(Wait(Session, InTasks, Call(Session, "asset.open", FAssetOpenRequest{PathToUtf8(Path)})));
	const auto Id = State.Document;
	Check(!State.bDirty && !State.bReadOnly && State.Type == "hyperion.textureasset");
	Check(Info(Wait(Session, InTasks, Call(Session, "asset.open", FAssetOpenRequest{PathToUtf8(Path)}))).Document ==
	      Id);
	Gui.Set("name", WriteValue(std::string("Saved")));
	State = Info(Call(Session, "asset.rename", FAssetRenameRequest{Id, State.Generation, "Saved"}));
	Check(State.Generation == Gui.Generation() && State.bDirty == Gui.IsDirty() &&
	      State.Name == ReadValue<std::string>(Gui.Get("name")));
	Failure(Call(Session, "asset.rename", FAssetRenameRequest{Id, 1, "Stale"}), "stale_revision");
	const auto Save = Call(Session, "asset.save", FAssetMutationRequest{Id, State.Generation});
	Failure(Call(Session, "asset.save", FAssetMutationRequest{Id, State.Generation}), "busy");
	Failure(Call(Session, "asset.close", FAssetCloseRequest{Id, State.Generation, true}), "busy");
	State = Info(Call(Session, "asset.rename", FAssetRenameRequest{Id, State.Generation, "Later"}));
	State = Info(Wait(Session, InTasks, Save));
	Check(State.Name == "Later" && State.bDirty && !State.bSaving);
	Check(InAssets.LoadAsync<FTextureAsset>(Path).Get(InTasks)->Name == "Saved");
	State = Info(Call(Session, "asset.undo", FAssetMutationRequest{Id, State.Generation}));
	Check(!State.bDirty && State.Name == "Saved");
	State = Info(Call(Session, "asset.redo", FAssetMutationRequest{Id, State.Generation}));
	Check(State.bDirty && State.Name == "Later");
	const auto Encoding = Call(Session, "texture.set_encoding",
	                           FAssetEncodingRequest{Id, State.Generation, EMaterialTextureEncoding::Srgb});
	Failure(Call(Session, "asset.undo", FAssetMutationRequest{Id, State.Generation}), "busy");
	State = Info(Wait(Session, InTasks, Encoding));
	State = Info(Wait(Session, InTasks, Call(Session, "asset.save", FAssetMutationRequest{Id, State.Generation})));
	const auto Saved = InAssets.LoadAsync<FTextureAsset>(Path).Get(InTasks);
	Check(Saved->Mips == BuildTextureAsset("Later", EMaterialTextureEncoding::Srgb, Texture.Mips.front()).Mips);
	Check(Saved->Encoding == EMaterialTextureEncoding::Srgb && !State.bDirty);
	State = Info(Call(Session, "asset.undo", FAssetMutationRequest{Id, State.Generation}));
	Check(State.bDirty);
	Failure(Call(Session, "asset.close", FAssetCloseRequest{Id, State.Generation, false}), "dirty_document");
	// An independent writer changes the on-disk digest, preserving identity.
	const auto Current = InAssets.LoadAsync(Path).Get(InTasks);
	InFiles.WriteAtomic(Path, EncodeAsset(RecordType<FTextureAsset>(), &Texture, Current->Header).Bytes);
	Failure(Wait(Session, InTasks, Call(Session, "asset.save", FAssetMutationRequest{Id, State.Generation})),
	        "save_failed");
	Check(ReadValue<bool>(Field(
	    Field(Call(Session, "asset.close", FAssetCloseRequest{Id, State.Generation, true}), "result"), "closed")));
	Failure(Call(Session, "asset.info", FAssetDocumentRequest{Id}), "not_found");
	Provider.Drain();
}

void CheckPluginLifetime()
{
	FApplicationHost Host(2, 1);
	auto Files = std::make_shared<FMemoryFileSystem>();
	FIOService IO(Host.GetTasks(), Files);
	FAssetService Assets(IO);
	Assets.Types().Register<FTextureAsset>();
	const auto Texture = Fixture();
	Files->WriteAtomic(Assets.NormalizePath("shutdown.hasset"),
	                   EncodeAsset(RecordType<FTextureAsset>(), &Texture).Bytes);
	Host.GetServices().AddExternal(Assets);
	FPluginRegistry Registry;
	RegisterAutomationServices(Registry);
	RegisterAssetAutomation(Registry);
	Host.Start(Registry, {{"automation-session", "automation-assets"}});
	auto& Session = Host.GetServices().Require<FAutomationSession>();
	auto State =
	    Info(Wait(Session, Host.GetTasks(), Call(Session, "asset.open", FAssetOpenRequest{"shutdown.hasset"})));
	State = Info(Call(Session, "asset.rename", FAssetRenameRequest{State.Document, State.Generation, "Drained"}));
	const auto Pending = Call(Session, "asset.save", FAssetMutationRequest{State.Document, State.Generation});
	Check(Text(Pending, "status") == "running");
	Host.Stop();
	Host.GetServices().Require<FApplicationControl>().RethrowFailure();
	Check(Assets.LoadAsync<FTextureAsset>("shutdown.hasset").Get(Host.GetTasks())->Name == "Drained");
	Assets.Drain();
}

void CheckRegistrationFailure()
{
	class FFailingPlugin final : public FPlugin
	{
	public:
		void Start(FPluginContext& InContext) override
		{
			auto& Catalog = InContext.Require<FOperationCatalog>();
			InContext.Defer(
			    [&Catalog]
			    {
				    Catalog.UnregisterOwner("automation-assets");
			    });
			RegisterAssetOperations(Catalog, nullptr);
			throw std::runtime_error("Injected provider startup failure after registration");
		}
	};

	FApplicationHost Host(2, 1);
	FPluginRegistry Registry;
	RegisterAutomationServices(Registry);
	FPluginDescriptor Failing;
	Failing.Id = "failing-adapter";
	Failing.Dependencies = {"automation-catalog"};
	Failing.Before = {"automation-session"};
	Failing.Requires = {typeid(FOperationCatalog)};
	Failing.Create = []
	{
		return std::make_unique<FFailingPlugin>();
	};
	Registry.Add(std::move(Failing));
	Host.Start(Registry, {{"failing-adapter", "automation-session"}});
	Check(Host.GetPlugins().IsActive("automation-session") && !Host.GetPlugins().IsActive("failing-adapter"));
	Check(Host.GetServices().Require<FOperationCatalog>().Size() == 0);
	Host.Stop();
}

void WriteFixture(const std::filesystem::path& InRoot)
{
	std::filesystem::create_directories(InRoot / "Game");
	std::filesystem::create_directories(InRoot / "ReadOnly");
	FLocalFileSystem Files;
	const auto Texture = Fixture();
	Files.WriteAtomic(InRoot / "Game/Texture.hasset", EncodeAsset(RecordType<FTextureAsset>(), &Texture).Bytes);
	auto Secondary = Texture;
	Secondary.Name = "Secondary";
	Files.WriteAtomic(InRoot / "Game/Secondary.hasset", EncodeAsset(RecordType<FTextureAsset>(), &Secondary).Bytes);
	Files.WriteAtomic(InRoot / "ReadOnly/Texture.hasset", EncodeAsset(RecordType<FTextureAsset>(), &Texture).Bytes);
}

class FRootObserver final : public IContentRootParticipant
{
public:
	FContentRootParticipantState State;
	int Released{};
	int Changed{};

	FContentRootParticipantState ContentRootState() const override
	{
		return State;
	}

	void ReleaseContentRoot() override
	{
		++Released;
	}

	void ContentRootChanged() override
	{
		++Changed;
	}
};

void CheckRootPluginLifetime(const std::filesystem::path& InRoot)
{
	FApplicationHost Host(2, 1);
	auto Files = CreateContentFileSystem(InRoot / "Engine");
	FIOService IO(Host.GetTasks(), Files);
	FAssetService Assets(IO);
	RegisterSceneAssetTypes(Assets.Types());
	FContentRootService Roots(Host.GetTasks(), *Files, Assets);
	Host.GetServices().AddExternal(Assets);
	Host.GetServices().AddExternal(Roots);
	FPluginRegistry Registry;
	RegisterAutomationServices(Registry);
	RegisterAssetAutomation(Registry);
	Host.Start(Registry, {{"automation-session", "automation-assets"}});
	auto& Session = Host.GetServices().Require<FAutomationSession>();
	Check(Text(Call(Session, "content.root.set", FContentRootRequest{PathToUtf8(InRoot / "Game"), 0}), "status") ==
	      "completed");
	(void)Info(Wait(Session, Host.GetTasks(), Call(Session, "asset.open", FAssetOpenRequest{"/Game/Texture.hasset"})));
	Host.Stop();
	Host.GetServices().Require<FApplicationControl>().RethrowFailure();
	// Scoped unregistration must leave no callback into the destroyed provider.
	Roots.Clear({1});
	Roots.Set({PathToUtf8(InRoot / "ReadOnly"), 2});
	Check(Assets.LoadAsync<FTextureAsset>("/Game/Texture.hasset").Get(Host.GetTasks())->Name == "Original");
	Assets.Drain();
}

void CheckUnsetRootOperations(FAssetAutomation& InProvider, FAutomationSession& InSession, FContentRootService& InRoots)
{
	for (const auto* Path : {"/Game", "/Game/", "/Game/Texture.hasset", "/Game/../Engine/Texture.hasset"})
	{
		bool bRejected{};
		try
		{
			(void)InProvider.Open({Path});
		}
		catch (const FAutomationError& Error)
		{
			bRejected = Error.Code == ContentRootErrors::RootUnset &&
			            std::string_view(Error.what()) ==
			                "Select an asset directory with content.root.set before opening Game assets";
		}
		Check(bRejected);
		const auto Result = Call(InSession, "asset.open", FAssetOpenRequest{Path});
		Failure(Result, "root_unset");
		Check(Text(Field(Result, "error"), "message") ==
		      "Select an asset directory with content.root.set before opening Game assets");
	}
	const auto Listing = Call(InSession, "content.directory.list", FContentDirectoryQuery{});
	Failure(Listing, "root_unset");
	Check(Text(Field(Listing, "error"), "message") == "Select Game content with content.root.set");
	Check(Text(Call(InSession, "content.directory.list", FContentDirectoryQuery{0, "/Engine"}), "status") ==
	      "completed");
	Check(InProvider.List({}).Documents.empty());
	Check(InRoots.Info().Generation == 0 && InRoots.Info().Directory.empty());
}

void CheckRootOperations(FTaskSystem& InTasks)
{
	const auto Root = std::filesystem::temp_directory_path() / ("HyperionRoots-" + CreateAutomationIdentity());
	WriteFixture(Root);
	std::filesystem::create_directories(Root / "Engine");
	{
		auto Files = CreateContentFileSystem(Root / "Engine");
		FIOService IO(InTasks, Files);
		FAssetService Assets(IO);
		RegisterSceneAssetTypes(Assets.Types());
		FContentRootService Roots(InTasks, *Files, Assets);
		FAssetAutomation Provider(Assets, InTasks, &Roots);
		FRootObserver Observer;
		Roots.RegisterParticipant(Provider);
		Roots.RegisterParticipant(Observer);
		FOperationCatalog Catalog;
		RegisterAssetOperations(Catalog, &Provider);
		RegisterContentRootOperations(Catalog, &Roots);
		RegisterContentQueries(Catalog, &Assets, &Roots, "automation-assets");
		Catalog.Seal();
		FAutomationSession Session(Catalog);
		Check(Roots.Info().Directory.empty() && Roots.Info().Generation == 0);
		CheckUnsetRootOperations(Provider, Session, Roots);
		Roots.Set({PathToUtf8(Root / "Game"), 0});
		std::ofstream(Root / "Game/Broken.hasset") << "invalid header";
		std::filesystem::create_directories(Root / "Game/Empty");
		const auto PageResult = Call(Session, "content.directory.list", FContentDirectoryQuery{1, "/Game", 0, 1});
		Check(Text(PageResult, "status") == "completed");
		const auto Page = std::static_pointer_cast<FContentDirectoryPage>(
		    ReadRecordWire(RecordType<FContentDirectoryPage>(), Field(PageResult, "result")));
		Check(Page->Total == 4 && Page->Entries.size() == 1 && Page->Next == 1u);
		Check(Page->Entries[0].Path == "/Game/Broken.hasset" && Page->Entries[0].State == "native_unindexed");
		Failure(Call(Session, "content.directory.list", FContentDirectoryQuery{0}), "stale_revision");
		Failure(Call(Session, "content.directory.list", FContentDirectoryQuery{1, "/Game/../Engine"}),
		        "invalid_arguments");
		Failure(Call(Session, "content.directory.list", FContentDirectoryQuery{1, PathToUtf8(Root)}),
		        "invalid_arguments");
		Failure(Call(Session, "content.directory.list", FContentDirectoryQuery{1, "/Game", 0, 101}),
		        "invalid_arguments");
		Check(!ReadValue<bool>(
		    Field(Field(Call(Session, "asset.workspace.policy", FContentRootQuery{}), "result"), "retainsFailed")));

		const auto Opening = Call(Session, "asset.open", FAssetOpenRequest{"/Game/Texture.hasset"});
		Failure(Call(Session, "content.root.clear", FContentRootClearRequest{1, true}), "busy");
		auto Document = Info(Wait(Session, InTasks, Opening));
		Observer.State.bDirty = true;
		Failure(Call(Session, "content.root.clear", FContentRootClearRequest{1}), "dirty_document");
		Check(Observer.Released == 1 &&
		      Info(Call(Session, "asset.info", FAssetDocumentRequest{Document.Document})).Document ==
		          Document.Document);
		Observer.State.bDirty = false;
		Document = Info(Call(Session, "asset.rename",
		                     FAssetRenameRequest{Document.Document, Document.Generation, "Old root save"}));
		Failure(Call(Session, "content.root.clear", FContentRootClearRequest{1}), "dirty_document");
		Roots.Set({PathToUtf8(Root / "Game/../Game"), 1});
		Check(Roots.Info().Generation == 1 && Observer.Released == 1);
		const auto Save = Call(Session, "asset.save", FAssetMutationRequest{Document.Document, Document.Generation});
		Failure(Call(Session, "content.root.clear", FContentRootClearRequest{1, true}), "busy");
		Document = Info(Wait(Session, InTasks, Save));
		const auto Encoding =
		    Call(Session, "texture.set_encoding",
		         FAssetEncodingRequest{Document.Document, Document.Generation, EMaterialTextureEncoding::Srgb});
		Failure(Call(Session, "content.root.clear", FContentRootClearRequest{1, true}), "busy");
		Document = Info(Wait(Session, InTasks, Encoding));
		auto Stale = Roots.Prepare(Root / "Game");
		Roots.Set({PathToUtf8(Root / "ReadOnly"), 1, true, true});
		Check(Roots.Info().Generation == 2 && Roots.Info().bReadOnly);
		Failure(Call(Session, "asset.save", FAssetMutationRequest{Document.Document, Document.Generation}),
		        "not_found");
		Failure(Call(Session, "content.root.clear", FContentRootClearRequest{1, true}), "stale_revision");
		bool bStaleRejected{};
		try
		{
			Roots.Commit(std::move(Stale), true);
		}
		catch (const FContentRootError& Error)
		{
			bStaleRejected = Error.Code == ContentRootErrors::StaleRevision;
		}
		Check(bStaleRejected);
		Document = Info(Wait(Session, InTasks, Call(Session, "asset.open", FAssetOpenRequest{"/Game/Texture.hasset"})));
		Check(Document.Name == "Original" && Document.bReadOnly);
		Failure(Call(Session, "asset.rename", FAssetRenameRequest{Document.Document, Document.Generation, "Denied"}),
		        "read_only");
		Roots.Clear({2});
		Check(Roots.Directory().empty() && Files->GetMounts().size() == 1);
		Check(Observer.Released == 3 && Observer.Changed == 3);
		Roots.Set({PathToUtf8(Root / "Game"), 3});
		Check(Assets.LoadAsync<FTextureAsset>("/Game/Texture.hasset").Get(InTasks)->Name == "Old root save");
		Roots.UnregisterParticipant(Observer);
		Roots.UnregisterParticipant(Provider);
	}
	CheckRootPluginLifetime(Root);
	std::filesystem::remove_all(Root);
}
} // namespace

void CheckAssetWorkflowAdapters();

int main(int InCount, char** InValues)
{
	try
	{
		if (InCount == 2)
		{
			WriteFixture(PathFromUtf8(InValues[1]));
			return 0;
		}
		FTaskSystem Tasks(2, 1);
		auto Files = std::make_shared<FMemoryFileSystem>();
		FIOService IO(Tasks, Files);
		FAssetService Assets(IO);
		CheckPropertyContracts();
		CheckDistinctSkyProperties(Assets, Tasks, *Files);
		CheckPropertyRangeEditing(Assets, Tasks, *Files);
		CheckEditing(Assets, Tasks, *Files);
		CheckWorkflowCompletion(Assets, Tasks);
		CheckAssetWorkflowAdapters();
		CheckMaterialNumericAutomation(Tasks, Assets, *Files);
		CheckPluginLifetime();
		CheckRegistrationFailure();
		CheckRootOperations(Tasks);
		Assets.Drain();
		Tasks.Shutdown();
		std::cout << "Asset automation shared semantics and lifecycle passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
