#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Core/ContentHash.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <thread>

namespace
{
using namespace Hyperion;

FAssetImporter HostImporter(const std::shared_ptr<std::atomic<std::uint32_t>>& InCalls)
{
	return {"test.host-texture",
	        1,
	        &RecordType<FTextureAsset>(),
	        {".host_texture"},
	        [InCalls](FAssetImportContext& InContext) -> std::shared_ptr<void>
	        {
		        HYP_CHECK(InContext.Bytes && InContext.Bytes->size() == 1);
		        ++*InCalls;
		        return std::make_shared<FTextureAsset>(
		            BuildTextureAsset("host-default", EMaterialTextureEncoding::Linear, {1, 1, {0, 255, 0, 255}}));
	        },
	        EAssetImporterExposure::Workspace,
	        "Host-selected test texture.",
	        {}};
}

struct FFixture
{
	std::filesystem::path Root = std::filesystem::current_path() / "asset-service-importers" / CreateIdentifier();
	FApplicationHost Host{2, 1};

	explicit FFixture(std::optional<std::vector<FAssetImporter>> InImporters = {}, bool bInDisableAssets = false)
	{
		std::filesystem::create_directories(Root / "Engine");
		std::filesystem::create_directories(Root / "Game");
		FPluginRegistry Registry;
		FAssetServiceOptions Options;
		Options.EngineContent = Root / "Engine";
		Options.AssetRoot = Root / "Game";
		Options.Importers = std::move(InImporters);
		auto LocalType = RecordType<FTextureAsset>();
		if (Options.Importers && !Options.Importers->empty())
		{
			Options.Importers->front().Type = &LocalType;
		}
		RegisterAssetServices(Registry, std::move(Options));
		// Composition must retain metadata before its delayed plugin Start.
		LocalType.Id.clear();
		RegisterAutomationServices(Registry);
		RegisterAssetAutomation(Registry);
		FPluginSelection Selection;
		Selection.Requested = {"assets", "automation-assets", "automation-session"};
		if (bInDisableAssets)
		{
			Selection.Disabled = {"assets"};
		}
		Host.Start(Registry, Selection);
	}

	void Close()
	{
		Host.Stop();
		Host.GetServices().Require<FApplicationControl>().RethrowFailure();
		HYP_CHECK(!Host.GetServices().Find<FAssetImportWorkspace>());
	}

	FArchiveNode Call(std::string_view InOperation, const FArchiveNode& InArguments)
	{
		auto& Session = Host.GetServices().Require<FAutomationSession>();
		auto Response = Session.Call(InOperation, InArguments);
		const auto First = ReadAutomationResponse(Response);
		if (First.Job)
		{
			HYP_CHECK(!First.Job->bCancellable);
			const std::string Job(First.Job->Id);
			const auto End = std::chrono::steady_clock::now() + std::chrono::seconds(20);
			while (ReadAutomationResponse(Response).Status == EAutomationStatus::Running &&
			       std::chrono::steady_clock::now() < End)
			{
				Host.GetTasks().PumpMain();
				std::this_thread::yield();
				Response = Session.GetJob(Job);
			}
			const auto Completed = ReadAutomationResponse(Response);
			HYP_CHECK(Completed.Status == EAutomationStatus::Completed);
			return *Completed.Job->Outcome;
		}
		return Response;
	}

	template<class T> T Result(std::string_view InOperation, const FArchiveNode& InArguments)
	{
		const auto Response = Call(InOperation, InArguments);
		const auto Object = ReadRecordWire(RecordType<T>(), ReadAutomationResponse(Response).CompletedResult());
		return *static_cast<const T*>(Object.get());
	}

	FImportRequest Request()
	{
		auto& IO = Host.GetServices().Require<FIOService>();
		IO.WriteAsync(Root / "Color.host_texture", {std::byte{1}}).Get(Host.GetTasks());
		FImportRequest Request;
		Request.Generation = Host.GetServices().Require<FContentRootService>().Info().Generation;
		Request.Source = PathToUtf8(Root / "Color.host_texture");
		Request.Output = "/Game/Selected.hasset";
		Request.Name = "HostRequestedName";
		return Request;
	}
};

void SelectedImporter()
{
	const auto Calls = std::make_shared<std::atomic<std::uint32_t>>(0);
	FFixture Fixture(std::vector<FAssetImporter>{HostImporter(Calls)});
	auto& Workspace = Fixture.Host.GetServices().Require<FAssetImportWorkspace>();
	auto& Session = Fixture.Host.GetServices().Require<FAutomationSession>();
	HYP_CHECK(WriteJson(Session.GetCatalog().Search("asset.import")).find("asset.import.capabilities") !=
	          std::string::npos);
	HYP_CHECK(Session.GetCatalog().Find("asset.import").Info.Version == 1);
	const auto Capabilities = Fixture.Result<FImportCapabilities>("asset.import.capabilities", ParseJson("{}"));
	const auto SelectedCapabilities = Workspace.GetCapabilities();
	HYP_CHECK(WriteJson(WriteRecordWire(RecordType<FImportCapabilities>(), &Capabilities)) ==
	          WriteJson(WriteRecordWire(RecordType<FImportCapabilities>(), &SelectedCapabilities)));
	HYP_CHECK(Capabilities.Formats.size() == 1 && Capabilities.Formats[0].Type == RecordType<FTextureAsset>().Id);
	HYP_CHECK(Capabilities.Formats[0].Extensions == std::vector<std::string>{".host_texture"});
	const auto Request = Fixture.Request();
	const auto Arguments = WriteRecordWire(RecordType<FImportRequest>(), &Request);
	const auto Validation = Fixture.Result<FImportValidation>("asset.import.validate", Arguments);
	HYP_CHECK(Validation.Type == RecordType<FTextureAsset>().Id);
	const auto Imported = Fixture.Result<FImportResult>("asset.import", Arguments);
	HYP_CHECK(Imported.WrittenAssets == 1 && !Imported.bUpToDate && Calls->load() == 1);
	const auto Texture = Fixture.Host.GetServices()
	                         .Require<FAssetService>()
	                         .LoadAsync<FTextureAsset>(Request.Output)
	                         .Get(Fixture.Host.GetTasks());
	HYP_CHECK(Texture->Name == Request.Name && Texture->Mips.front().Width == 1);
	auto Disabled = Request;
	Disabled.Source = PathToUtf8(Fixture.Root / "Color.png");
	const auto Rejected =
	    Fixture.Call("asset.import.validate", WriteRecordWire(RecordType<FImportRequest>(), &Disabled));
	HYP_CHECK(ReadAutomationResponse(Rejected).IsFailed());
	Fixture.Close();
}

void DefaultAndEmptySelections()
{
	FFixture Default;
	const auto Defaults = Default.Result<FImportCapabilities>("asset.import.capabilities", ParseJson("{}"));
	const auto Expected = FAssetImportWorkspace::Capabilities();
	HYP_CHECK(WriteJson(WriteRecordWire(RecordType<FImportCapabilities>(), &Defaults)) ==
	          WriteJson(WriteRecordWire(RecordType<FImportCapabilities>(), &Expected)));
	Default.Close();
	FFixture Empty(std::vector<FAssetImporter>{});
	HYP_CHECK(Empty.Host.GetPlugins().IsActive("assets"));
	HYP_CHECK(Empty.Result<FImportCapabilities>("asset.import.capabilities", ParseJson("{}")).Formats.empty());
	const auto Request = Empty.Request();
	const auto Rejected = Empty.Call("asset.import", WriteRecordWire(RecordType<FImportRequest>(), &Request));
	HYP_CHECK(ReadAutomationResponse(Rejected).IsFailed());
	HYP_CHECK(!Empty.Host.GetServices().Require<FIOService>().FileSystem()->Exists(Request.Output));
	Empty.Close();
}

void UnavailableSelections()
{
	for (const bool bDisabled : {false, true})
	{
		const auto Calls = std::make_shared<std::atomic<std::uint32_t>>(0);
		auto Invalid = HostImporter(Calls);
		Invalid.Id.clear();
		FFixture Fixture(std::vector<FAssetImporter>{std::move(Invalid)}, bDisabled);
		HYP_CHECK(!Fixture.Host.GetPlugins().IsActive("assets"));
		HYP_CHECK(Fixture.Host.GetPlugins().IsActive("automation-session"));
		HYP_CHECK(!Fixture.Host.GetServices().Find<FAssetImportWorkspace>());
		const auto Response = Fixture.Call("asset.import.capabilities", ParseJson("{}"));
		const auto Failure = ReadAutomationResponse(Response);
		HYP_CHECK(Failure.IsFailed() && Failure.Error->Code == "unavailable" && Calls->load() == 0);
		if (!bDisabled)
		{
			HYP_CHECK(std::any_of(Fixture.Host.GetPlugins().GetDiagnostics().begin(),
			                      Fixture.Host.GetPlugins().GetDiagnostics().end(),
			                      [](const auto& InDiagnostic)
			                      {
				                      return InDiagnostic.Id == "assets" && InDiagnostic.bStartupFailure;
			                      }));
		}
		Fixture.Close();
	}
}
} // namespace

int main()
{
	try
	{
		SelectedImporter();
		DefaultAndEmptySelections();
		UnavailableSelections();
		std::cout << "Host importer selection, discovery, invocation and lifecycle passed\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
