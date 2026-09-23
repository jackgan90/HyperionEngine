#include "AssetOperations.h"
#include "ContentRootOperations.h"
#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <source_location>
#include <thread>

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
		Catalog.Seal();
		FAutomationSession Session(Catalog);
		Check(Roots.Info().Directory.empty() && Roots.Info().Generation == 0);
		Failure(Call(Session, "asset.open", FAssetOpenRequest{"/Game/Texture.hasset"}), "root_unset");
		Roots.Set({PathToUtf8(Root / "Game"), 0});
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
			bStaleRejected = Error.Code == "stale_revision";
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
		CheckEditing(Assets, Tasks, *Files);
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
