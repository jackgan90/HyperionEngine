#include "Hyperion/AssetEditing/AssetDocument.h"
#include "Hyperion/AssetImport/AssetSourceJson.h"
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/LogSupport.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <thread>

void CheckImportStates();
void CheckContentPathContracts();
void CheckImportValidation(Hyperion::FIOService& InIO, Hyperion::FAssetImportWorkspace& InWorkspace,
                           Hyperion::FImportRequest InRequest);

namespace
{
using namespace Hyperion;

template<class TFunction> void Reject(TFunction InFunction)
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
	HYP_CHECK(bRejected);
}

struct FFixture
{
	FTaskSystem Tasks{2, 1};
	std::filesystem::path Root = std::filesystem::current_path() / "import-workspace" / CreateIdentifier();
	std::shared_ptr<FMountedFileSystem> Files;
	std::unique_ptr<FIOService> IO;
	std::unique_ptr<FAssetService> Assets;
	std::unique_ptr<FContentRootService> Content;
	std::unique_ptr<FAssetImportWorkspace> Imports;

	FFixture()
	{
		std::filesystem::create_directories(Root / "Engine");
		std::filesystem::create_directories(Root / "Game");
		Files = CreateContentFileSystem(Root / "Engine", true);
		IO = std::make_unique<FIOService>(Tasks, Files);
		Assets = std::make_unique<FAssetService>(*IO);
		RegisterSceneAssetTypes(Assets->Types());
		Content = std::make_unique<FContentRootService>(Tasks, *Files, *Assets);
		Imports = std::make_unique<FAssetImportWorkspace>(*IO, *Assets, *Content);
		Content->RegisterParticipant(*Imports);
		Content->Change(Root / "Game");
		const FImage Image{2, 2, EColorSpace::Srgb, {0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1}};
		IO->WriteAsync(Root / "Color.png", EncodePng(Image)).Get(Tasks);
		const auto Brdf = BuildEnvironmentBrdf(2, 4);
		IO->WriteAsync("/Engine/Textures/EnvironmentBrdf.hasset", EncodeAsset(RecordType<FTextureAsset>(), &Brdf).Bytes)
		    .Get(Tasks);
		const std::string Hdr = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 2\n";
		const auto Bytes = std::as_bytes(std::span(Hdr));
		FBytes Sky(Bytes.begin(), Bytes.end());
		for (int Pixel = 0; Pixel < 2; ++Pixel)
		{
			Sky.insert(Sky.end(), {std::byte{128}, std::byte{128}, std::byte{128}, std::byte{129}});
		}
		IO->WriteAsync(Root / "Sky.hdr", std::move(Sky)).Get(Tasks);
		SaveImage(Root / "Sky.exr", FImage{2, 1, EColorSpace::Linear, {1, 1, 1, 1, 1, 1, 1, 1}});
	}

	~FFixture()
	{
		Content->UnregisterParticipant(*Imports);
		Imports->Drain();
	}

	FImportRequest Request(std::string InSource, std::string InOutput)
	{
		return {Content->Info().Generation, PathToUtf8(Root / InSource), "/Game/" + InOutput, "/Game"};
	}

	FImportResult Wait(const std::shared_ptr<const FImportTask>& InTask)
	{
		const auto End = std::chrono::steady_clock::now() + std::chrono::seconds(20);
		while (InTask->Info.Status == EImportTaskState::Running && std::chrono::steady_clock::now() < End)
		{
			Imports->Update();
			Tasks.PumpMain();
			std::this_thread::yield();
		}
		if (InTask->Info.Status != EImportTaskState::Completed)
		{
			throw std::runtime_error("Import did not complete: " + InTask->Info.Error);
		}
		HYP_CHECK(InTask->Info.Result->Warning.empty());
		return *InTask->Info.Result;
	}
};

void CheckImage(FFixture& InFixture)
{
	auto Request = InFixture.Request("Color.png", "Color.hasset");
	const auto Before = InFixture.IO->Statistics().Writes.load();
	(void)InFixture.Imports->Validate(Request);
	HYP_CHECK(InFixture.IO->Statistics().Writes.load() == Before);
	const auto Task = InFixture.Imports->Start(Request);
	HYP_CHECK(InFixture.Imports->ContentRootState().bBusy);
	Reject(
	    [&]
	    {
		    InFixture.Content->Clear({Request.Generation, true});
	    });
	const auto First = InFixture.Wait(Task);
	HYP_CHECK(First.WrittenAssets == 1 && !First.bUpToDate);
	const auto Texture = InFixture.Assets->LoadAsync<FTextureAsset>("/Game/Color.hasset").Get(InFixture.Tasks);
	HYP_CHECK(Texture->Encoding == EMaterialTextureEncoding::Srgb && Texture->Mips.size() == 2);
	const auto Again = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Again.Asset.Id == First.Asset.Id && Again.bUpToDate && Again.WrittenAssets == 0);
	Request.TextureEncoding = EMaterialTextureEncoding::Linear;
	Request.Name = "Linear texture";
	const auto Changed = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Changed.Asset.Id == First.Asset.Id && Changed.WrittenAssets == 1);
	const auto Linear = InFixture.Assets->LoadAsync<FTextureAsset>("/Game/Color.hasset").Get(InFixture.Tasks);
	HYP_CHECK(Linear->Encoding == EMaterialTextureEncoding::Linear && Linear->Name == Request.Name);
	HYP_CHECK(InFixture.Imports->Get({Changed.Task}).Result->Asset == Changed.Asset);
	HYP_CHECK(InFixture.Imports->List().Total == 3);
	InFixture.IO->WriteAsync("/Game/Local.png", EncodePng(FImage{1, 1, EColorSpace::Srgb, {1, 0, 0, 1}}))
	    .Get(InFixture.Tasks);
	auto Local = InFixture.Request("Game/Local.png", "Local.hasset");
	Local.SourceRoot = PathToUtf8(InFixture.Root / "Game");
	Local.SourceId = "local-image";
	HYP_CHECK(InFixture.Wait(InFixture.Imports->Start(Local)).WrittenAssets == 1);
	Reject(
	    [&]
	    {
		    InFixture.Imports->List({0, 33});
	    });
	Request.Generation = 0;
	Reject(
	    [&]
	    {
		    InFixture.Imports->Start(Request);
	    });
}

void CheckSky(FFixture& InFixture)
{
	auto Request = InFixture.Request("Sky.hdr", "Nested/Sky.hasset");
	Request.Library.clear();
	Request.Sky = FEnvironmentBakeSettings{8, 4, 8};
	const auto First = InFixture.Wait(InFixture.Imports->Start(Request));
	const auto Again = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Again.bUpToDate && Again.WrittenAssets == 0);
	Request.Sky->RadianceSize = 16;
	const auto Changed = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Changed.Asset.Id == First.Asset.Id && !Changed.bUpToDate);
	const auto Sky = InFixture.Assets->LoadAsync<FSkyAsset>("/Game/Nested/Sky.hasset").Get(InFixture.Tasks);
	HYP_CHECK(PathFromUtf8(Sky->Radiance.Path).parent_path() == "/Game/Nested");
	HYP_CHECK(PathFromUtf8(Sky->Specular.Path).parent_path() == "/Game/Nested");
	const auto Radiance =
	    InFixture.Assets->LoadAsync<FTextureAsset>(PathFromUtf8(Sky->Radiance.Path)).Get(InFixture.Tasks);
	HYP_CHECK(Radiance->Dimension == ETextureDimension::Cube && Radiance->Mips.front().Width == 16);
	Request.Sky->RadianceSize = 3;
	Reject(
	    [&]
	    {
		    InFixture.Imports->Start(Request);
	    });
	Request.Sky.reset();
	Request.TextureEncoding = EMaterialTextureEncoding::Linear;
	Reject(
	    [&]
	    {
		    InFixture.Imports->Start(Request);
	    });
	Request = InFixture.Request("Sky.exr", "Exr.hasset");
	Request.Sky = FEnvironmentBakeSettings{8, 4, 8};
	HYP_CHECK(InFixture.Wait(InFixture.Imports->Start(Request)).WrittenAssets > 0);
}

void CheckContentFailureDiagnostic()
{
	FFixture Fixture;
	FTestLogCapture Logs(Fixture.Root / "ContentDiagnostics");

	struct FFailingParticipant final : IContentRootParticipant
	{
		FContentRootParticipantState ContentRootState() const override
		{
			return {};
		}

		void ReleaseContentRoot() override
		{
			throw std::runtime_error("injected retirement failure");
		}

		void ContentRootChanged() override
		{
		}
	} Participant;

	Fixture.Content->RegisterParticipant(Participant);
	std::filesystem::create_directories(Fixture.Root / "Other");
	bool bFailed{};
	try
	{
		Fixture.Content->Change(Fixture.Root / "Other");
	}
	catch (const FContentRootError& Failure)
	{
		bFailed = Failure.Code == "content_failed";
	}
	Fixture.Content->UnregisterParticipant(Participant);
	HYP_CHECK(bFailed);
	HYP_CHECK(Logs.Count(ELogLevel::Error, {"Content root transition failed;", PathToUtf8(Fixture.Root / "Game"),
	                                        PathToUtf8(Fixture.Root / "Other"), "consumer retirement",
	                                        "requires restart", "injected retirement failure"}) == 1);
}

void CheckSaveDiagnostics(FFixture& InFixture)
{
	InFixture.Wait(InFixture.Imports->Start(InFixture.Request("Color.png", "SaveDiagnostic.hasset")));
	FTestLogCapture Logs(InFixture.Root / "SaveDiagnostics");
	const auto Loaded = InFixture.Assets->LoadAsync("/Game/SaveDiagnostic.hasset").Get(InFixture.Tasks);
	FAssetEditDocument Current(Loaded);
	FAssetEditDocument Stale(Loaded);
	Current.Set("name", WriteValue(std::string("Updated diagnostic asset")));
	Stale.Set("name", WriteValue(std::string("Stale diagnostic asset")));
	for (auto* Document : {&Current, &Stale})
	{
		Document->Save(*InFixture.Assets);
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (Document->IsSaving() && std::chrono::steady_clock::now() < Deadline)
		{
			Document->PollSave();
			InFixture.Tasks.PumpMain();
			std::this_thread::yield();
		}
		HYP_CHECK(!Document->IsSaving());
	}
	HYP_CHECK(Current.Error.empty() && !Stale.Error.empty() && Stale.IsDirty());
	HYP_CHECK(Logs.Count(ELogLevel::Info,
	                     {"Asset save completed;", "SaveDiagnostic.hasset", Loaded->Header.Id, "revision="}) == 1);
	HYP_CHECK(Logs.Count(ELogLevel::Error,
	                     {"Asset save failed;", "SaveDiagnostic.hasset", Loaded->Header.Id, Stale.Error}) == 1);
	const auto Before = Logs.History->Count();
	for (int Index = 0; Index < 32; ++Index)
	{
		Current.PollSave();
		Stale.PollSave();
	}
	HYP_CHECK(Logs.History->Count() == Before);
}

void CheckFailedImport(FFixture& InFixture)
{
	FTestLogCapture Logs(InFixture.Root / "Diagnostics");
	InFixture.IO->WriteAsync(InFixture.Root / "Broken.png", FBytes{std::byte{0}}).Get(InFixture.Tasks);
	const auto Task = InFixture.Imports->Start(InFixture.Request("Broken.png", "Broken.hasset"));
	Reject(
	    [&]
	    {
		    InFixture.Wait(Task);
	    });
	HYP_CHECK(Task->Info.Status == EImportTaskState::Failed && !Task->Info.Error.empty());
	HYP_CHECK(!InFixture.Imports->ContentRootState().bBusy);
	HYP_CHECK(!InFixture.Files->Exists("/Game/Broken.hasset"));
	HYP_CHECK(Logs.Count(ELogLevel::Error,
	                     {"Import failed;", Task->Info.Task, Task->Info.Source, Task->Info.Output, "reason="}) == 1);
	const auto Succeeded = InFixture.Imports->Start(InFixture.Request("Color.png", "LogSuccess.hasset"));
	InFixture.Wait(Succeeded);
	HYP_CHECK(Logs.Count(ELogLevel::Info, {"Import completed;", Succeeded->Info.Task, Succeeded->Info.Output,
	                                       "written_assets=", "asset='"}) == 1);
	const auto Before = Logs.History->Count();
	for (int Index = 0; Index < 64; ++Index)
	{
		InFixture.Imports->Update();
		(void)InFixture.Imports->Get({Task->Info.Task});
	}
	HYP_CHECK(Logs.History->Count() == Before);
}

FImportDraftInfo WaitDraft(FFixture& InFixture, const std::string& InId)
{
	const auto End = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	for (;;)
	{
		InFixture.Imports->Update();
		InFixture.Tasks.PumpMain();
		auto State = InFixture.Imports->Draft({InId});
		if (State.Status != EImportDraftState::Preparing && State.Status != EImportDraftState::Publishing)
		{
			return State;
		}
		HYP_CHECK(std::chrono::steady_clock::now() < End);
		std::this_thread::yield();
	}
}

void CheckGroupedImports(FFixture& InFixture)
{
	auto Request = InFixture.Request("Color.png", "Grouped/Custom.hasset");
	Request.Library.clear();
	Request.bCreateFolder = true;
	const auto BeforePreview = InFixture.IO->Statistics().Writes.load();
	HYP_CHECK(InFixture.Imports->Validate(Request).Folder == "/Game/Grouped/Color");
	const auto Preview = WaitDraft(InFixture, InFixture.Imports->PrepareDraft(Request).Draft);
	HYP_CHECK(Preview.Status == EImportDraftState::Ready);
	HYP_CHECK(InFixture.IO->Statistics().Writes.load() == BeforePreview);
	HYP_CHECK(!InFixture.Files->Exists("/Game/Grouped"));
	InFixture.Imports->DiscardDraft({Preview.Draft, Preview.Generation, true});
	const auto First = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(First.Asset.Path == "/Game/Grouped/Color/Custom.hasset");
	HYP_CHECK(First.WrittenAssets == 1);
	const auto Writes = InFixture.IO->Statistics().Writes.load();
	const auto Again = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Again.Asset == First.Asset && Again.bUpToDate && Again.WrittenAssets == 0);
	HYP_CHECK(InFixture.IO->Statistics().Writes.load() == Writes);
	InFixture.Content->UnregisterParticipant(*InFixture.Imports);
	InFixture.Imports->Drain();
	InFixture.Imports = std::make_unique<FAssetImportWorkspace>(*InFixture.IO, *InFixture.Assets, *InFixture.Content);
	InFixture.Content->RegisterParticipant(*InFixture.Imports);
	HYP_CHECK(InFixture.Wait(InFixture.Imports->Start(Request)).bUpToDate);
	Request.TextureEncoding = EMaterialTextureEncoding::Linear;
	const auto Changed = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Changed.Asset.Id == First.Asset.Id && Changed.Asset.Path == First.Asset.Path &&
	          Changed.WrittenAssets == 1);
	const auto Bytes = InFixture.IO->ReadAsync(InFixture.Root / "Color.png").Get(InFixture.Tasks);
	InFixture.IO->WriteAsync(InFixture.Root / "Other/Color.png", *Bytes).Get(InFixture.Tasks);
	auto Other = Request;
	Other.Source = PathToUtf8(InFixture.Root / "Other/Color.png");
	HYP_CHECK(InFixture.Imports->Validate(Other).Folder == "/Game/Grouped/Color_1");
	const auto Collision = InFixture.Wait(InFixture.Imports->Start(Other));
	HYP_CHECK(Collision.Asset.Path == "/Game/Grouped/Color_1/Custom.hasset");
	HYP_CHECK(Collision.Asset.Id != First.Asset.Id);
	// An unrelated folder occupies the basename in a new destination.
	std::filesystem::create_directories(InFixture.Root / "Game/Elsewhere/Color");
	Request.Output = "/Game/Elsewhere/Custom.hasset";
	const auto Elsewhere = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Elsewhere.Asset.Path == "/Game/Elsewhere/Color_1/Custom.hasset");
	std::filesystem::remove(InFixture.Root / "Game/Elsewhere/Color");
	HYP_CHECK(InFixture.Wait(InFixture.Imports->Start(Request)).Asset == Elsewhere.Asset);
	InFixture.IO->WriteAsync(InFixture.Root / "Color.png", EncodePng(FImage{1, 1, EColorSpace::Srgb, {1, 0, 0, 1}}))
	    .Get(InFixture.Tasks);
	const auto SourceChanged = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(SourceChanged.Asset.Id == Elsewhere.Asset.Id && SourceChanged.Asset.Path == Elsewhere.Asset.Path &&
	          SourceChanged.WrittenAssets == 1);
	InFixture.IO->WriteAsync(InFixture.Root / "Color.png", *Bytes).Get(InFixture.Tasks);
	// Multiple sky products stay together, and setting changes retain the folder.
	auto SkyRequest = InFixture.Request("Sky.hdr", "Grouped/Evening.hasset");
	SkyRequest.Library.clear();
	SkyRequest.bCreateFolder = true;
	SkyRequest.Sky = FEnvironmentBakeSettings{8, 4, 8};
	const auto SkyResult = InFixture.Wait(InFixture.Imports->Start(SkyRequest));
	HYP_CHECK(SkyResult.Asset.Path == "/Game/Grouped/Sky/Evening.hasset");
	const auto Sky = InFixture.Assets->LoadAsync<FSkyAsset>(PathFromUtf8(SkyResult.Asset.Path)).Get(InFixture.Tasks);
	HYP_CHECK(PathFromUtf8(Sky->Radiance.Path).parent_path() == "/Game/Grouped/Sky");
	HYP_CHECK(PathFromUtf8(Sky->Specular.Path).parent_path() == "/Game/Grouped/Sky");
	SkyRequest.Sky->RadianceSize = 16;
	HYP_CHECK(InFixture.Wait(InFixture.Imports->Start(SkyRequest)).Asset.Id == SkyResult.Asset.Id);
	HYP_CHECK(InFixture.Wait(InFixture.Imports->Start(SkyRequest)).bUpToDate);
}

void CheckGroupedDestinationEdges(FFixture& InFixture)
{
	auto Request = InFixture.Request("Color.png", "CaseCollision/Color.hasset");
	Request.Library.clear();
	Request.bCreateFolder = true;
	std::filesystem::create_directories(InFixture.Root / "Game/CaseCollision/color");
	HYP_CHECK(InFixture.Imports->Validate(Request).Folder == "/Game/CaseCollision/Color_1");
	HYP_CHECK(InFixture.Wait(InFixture.Imports->Start(Request)).Asset.Path ==
	          "/Game/CaseCollision/Color_1/Color.hasset");
	InFixture.IO->WriteAsync("/Game/CorruptMarker/Unrelated/.import-source", FBytes(129)).Get(InFixture.Tasks);
	Request.Output = "/Game/CorruptMarker/Color.hasset";
	HYP_CHECK(InFixture.Wait(InFixture.Imports->Start(Request)).Asset.Path == "/Game/CorruptMarker/Color/Color.hasset");
	HYP_CHECK(InFixture.IO->ReadAsync("/Game/CorruptMarker/Unrelated/.import-source").Get(InFixture.Tasks)->size() ==
	          129);
	const auto Bytes = InFixture.IO->ReadAsync(InFixture.Root / "Color.png").Get(InFixture.Tasks);
	InFixture.IO->WriteAsync(InFixture.Root / PathFromUtf8("Ä.png"), *Bytes).Get(InFixture.Tasks);
	std::filesystem::create_directories(InFixture.Root / PathFromUtf8("Game/Unicode/ä"));
	Request.Source = PathToUtf8(InFixture.Root / PathFromUtf8("Ä.png"));
	Request.Output = "/Game/Unicode/Result.hasset";
	HYP_CHECK(InFixture.Imports->Validate(Request).Folder == "/Game/Unicode/Ä_1");
	const auto Unicode = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Unicode.Asset.Path == "/Game/Unicode/Ä_1/Result.hasset");
	HYP_CHECK(InFixture.Assets->LoadAsync<FTextureAsset>(PathFromUtf8(Unicode.Asset.Path)).Get(InFixture.Tasks)->Name ==
	          "Ä");
	const auto UnicodeRepeat = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(UnicodeRepeat.bUpToDate && UnicodeRepeat.WrittenAssets == 0 && UnicodeRepeat.Asset == Unicode.Asset);
	for (const auto* Name : {"Trailing..png", "Trailing .png"})
	{
		InFixture.IO->WriteAsync(InFixture.Root / Name, *Bytes).Get(InFixture.Tasks);
		Request.Source = PathToUtf8(InFixture.Root / Name);
		Request.Output = "/Game/SafeNames/Result.hasset";
		const auto Result = InFixture.Wait(InFixture.Imports->Start(Request));
		HYP_CHECK(Result.Asset.Path == (std::string(Name) == "Trailing..png"
		                                    ? "/Game/SafeNames/Trailing/Result.hasset"
		                                    : "/Game/SafeNames/Trailing_1/Result.hasset"));
	}
	Request = InFixture.Request("Color.png", "ProductCase/sky_radiance_1.hasset");
	Request.Library.clear();
	const auto Existing = InFixture.Wait(InFixture.Imports->Start(Request));
	Request.Source = PathToUtf8(InFixture.Root / "Sky.hdr");
	Request.Output = "/Game/ProductCase/Sky.hasset";
	Request.Sky = FEnvironmentBakeSettings{8, 4, 8};
	InFixture.Wait(InFixture.Imports->Start(Request));
	const auto Sky = InFixture.Assets->LoadAsync<FSkyAsset>("/Game/ProductCase/Sky.hasset").Get(InFixture.Tasks);
	HYP_CHECK(Sky->Radiance.Path == "/Game/ProductCase/Sky_radiance_2.hasset");
	HYP_CHECK(InFixture.Assets->LoadAsync("/Game/ProductCase/sky_radiance_1.hasset").Get(InFixture.Tasks)->Header.Id ==
	          Existing.Asset.Id);
}

void CheckFailedGroupCleanup(FFixture& InFixture)
{
	auto Request = InFixture.Request("BrokenGroup.png", "FailedGroup/BrokenGroup.hasset");
	Request.Library.clear();
	Request.bCreateFolder = true;
	InFixture.IO->WriteAsync(InFixture.Root / "BrokenGroup.png", FBytes{std::byte{0}}).Get(InFixture.Tasks);
	for (unsigned Attempt = 0; Attempt < 2; ++Attempt)
	{
		Reject(
		    [&]
		    {
			    InFixture.Wait(InFixture.Imports->Start(Request));
		    });
		HYP_CHECK(!InFixture.Files->Exists("/Game/FailedGroup/BrokenGroup"));
	}
	const auto Bytes = InFixture.IO->ReadAsync(InFixture.Root / "Color.png").Get(InFixture.Tasks);
	InFixture.IO->WriteAsync(InFixture.Root / "BrokenGroup.png", *Bytes).Get(InFixture.Tasks);
	const auto Result = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Result.Asset.Path == "/Game/FailedGroup/BrokenGroup/BrokenGroup.hasset");
	InFixture.IO->WriteAsync(InFixture.Root / "BrokenGroup.png", FBytes{std::byte{0}}).Get(InFixture.Tasks);
	Reject(
	    [&]
	    {
		    InFixture.Wait(InFixture.Imports->Start(Request));
	    });
	HYP_CHECK(InFixture.Files->Exists(PathFromUtf8(Result.Asset.Path)));
	HYP_CHECK(!InFixture.Files->RemoveEmptyDirectory("/Game/FailedGroup/BrokenGroup"));
	HYP_CHECK(!InFixture.Files->RemoveEmptyDirectory(PathFromUtf8(Result.Asset.Path)));
	HYP_CHECK(InFixture.Files->Exists(PathFromUtf8(Result.Asset.Path)));
}

void CheckPanoramaDimensions(FFixture& InFixture)
{
	for (const std::string Extension : {".hdr", ".exr"})
	{
		const auto Source = "Square" + Extension;
		if (Extension == ".hdr")
		{
			const std::string Header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 2\n";
			const auto Bytes = std::as_bytes(std::span(Header));
			FBytes Image(Bytes.begin(), Bytes.end());
			for (int Pixel = 0; Pixel < 4; ++Pixel)
			{
				Image.insert(Image.end(), {std::byte{128}, std::byte{128}, std::byte{128}, std::byte{129}});
			}
			InFixture.IO->WriteAsync(InFixture.Root / Source, std::move(Image)).Get(InFixture.Tasks);
		}
		else
		{
			SaveImage(InFixture.Root / Source, FImage{2, 2, EColorSpace::Linear, std::vector<float>(16, 1.f)});
		}
		const auto Request = InFixture.Request(Source, "RejectedSky.hasset");
		const auto Before = InFixture.IO->Statistics().Writes.load();
		const auto State = WaitDraft(InFixture, InFixture.Imports->PrepareDraft(Request).Draft);
		HYP_CHECK(State.Status == EImportDraftState::Failed);
		HYP_CHECK(State.Error.find("2 x 2") != std::string::npos && State.Error.find("2:1") != std::string::npos);
		HYP_CHECK(InFixture.IO->Statistics().Writes.load() == Before);
		HYP_CHECK(!InFixture.Files->Exists("/Game/RejectedSky.hasset"));
		InFixture.Imports->DiscardDraft({State.Draft, State.Generation, true});
	}
}

void CheckDraftMetadataSchema()
{
	const auto Schema = RecordWireSchema(RecordType<FImportDraftInfo>());
	const auto& Fields =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Schema.Value).at("properties").Value);
	const auto& Dimension = std::get<FArchiveNode::FObject>(Fields.at("dimension").Value);
	HYP_CHECK(std::get<FArchiveNode::FArray>(Dimension.at("enum").Value).size() == 2);
	HYP_CHECK(std::get<FArchiveNode::FArray>(Dimension.at("oneOf").Value).size() == 2);
	const auto& Bytes = std::get<FArchiveNode::FObject>(Fields.at("pixelBytes").Value);
	HYP_CHECK(ReadValue<std::string>(Bytes.at("type")) == "string");
	HYP_CHECK(ReadValue<std::string>(Bytes.at("description")).find("mips and faces") != std::string::npos);
	HYP_CHECK(Fields.contains("width") && Fields.contains("height") && Fields.contains("details"));
	auto Texture = std::make_shared<FTextureAsset>();
	Texture->Name = "Cube";
	Texture->Dimension = ETextureDimension::Cube;
	Texture->Mips = {{2, 2, std::vector<std::uint8_t>(96)}, {1, 1, std::vector<std::uint8_t>(24)}};
	ValidateTextureAsset(*Texture);
	const FConvertedAsset Root{std::make_shared<FRecordDescriptor>(RecordType<FTextureAsset>()), Texture};
	FImportDraftInfo Info;
	DescribeImportRoot(Info, Root, {});
	HYP_CHECK(Info.Dimension == ETextureDimension::Cube && Info.PixelBytes == 120 && Info.Width == 2 &&
	          Info.Height == 2);
	HYP_CHECK(Info.Details == std::vector<std::string>{"Cube | pixel bytes: 120"});
}

void CheckDraftPagination()
{
	auto Model = std::make_shared<FModelAsset>();
	Model->Name = "Paged";
	FModelPrimitive Primitive;
	Primitive.Name = "Triangle";
	Primitive.Positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
	Primitive.Indices = {0, 1, 2};
	Primitive.Material = 0;
	Primitive.Id = "triangle";
	Model->Primitives.push_back(std::move(Primitive));
	Model->MaterialSlots.push_back(
	    {.Id = CreateIdentifier(), .Path = "/Game/Material.hasset", .TypeId = "hyperion.materialasset"});
	const auto Count = 2 * ImportDraftPreviewPageLimit + 5;
	for (std::uint32_t Index = 0; Index < Count; ++Index)
	{
		Model->Nodes.push_back({"Node " + std::to_string(Index), Identity(), {0}, {}, "node-" + std::to_string(Index)});
		Model->Roots.push_back(Index);
	}
	const FConvertedAsset Root{std::make_shared<FRecordDescriptor>(RecordType<FModelAsset>()), Model};
	std::vector<std::string> Observed;
	for (std::uint32_t Offset = 0; Offset < Count; Offset += ImportDraftPreviewPageLimit)
	{
		FImportDraftInfo Page;
		DescribeImportRoot(Page, Root, {"", Offset, ImportDraftPreviewPageLimit});
		HYP_CHECK(Page.TotalNodes == Count);
		HYP_CHECK(Page.Nodes.size() == std::min(ImportDraftPreviewPageLimit, Count - Offset));
		for (const auto& Node : Page.Nodes)
		{
			Observed.push_back(Node.Id);
		}
	}
	HYP_CHECK(Observed.size() == Count);
	for (std::uint32_t Index = 0; Index < Count; ++Index)
	{
		HYP_CHECK(Observed[Index] == Model->Nodes[Index].Id);
	}
	FImportDraftInfo Previous;
	DescribeImportRoot(Previous, Root, {"", ImportDraftPreviewPageLimit, ImportDraftPreviewPageLimit});
	HYP_CHECK(Previous.Nodes.front().Id == Observed[ImportDraftPreviewPageLimit]);
	HYP_CHECK(Previous.Nodes.back().Id == Observed[2 * ImportDraftPreviewPageLimit - 1]);
	HYP_CHECK(FImportDraftQuery{}.Limit == ImportDraftPreviewPageLimit);
}

void CheckDraftEditing(FFixture& InFixture)
{
	const auto Request = InFixture.Request("Color.png", "Draft.hasset");
	const auto Before = InFixture.IO->Statistics().Writes.load();
	auto State = InFixture.Imports->PrepareDraft(Request);
	HYP_CHECK(State.Status == EImportDraftState::Preparing);
	HYP_CHECK(InFixture.Imports->ContentRootState().bBusy);
	State = WaitDraft(InFixture, State.Draft);
	HYP_CHECK(State.Status == EImportDraftState::Ready && State.Width == 2 && State.Mips == 2);
	HYP_CHECK(State.Height == 2 && State.Dimension == ETextureDimension::Texture2D && State.PixelBytes == 20);
	HYP_CHECK(State.Details == std::vector<std::string>{"Texture2D | pixel bytes: 20"});
	const auto Wire = WriteRecordWire(RecordType<FImportDraftInfo>(), &State);
	const auto& WireFields = std::get<FArchiveNode::FObject>(Wire.Value);
	HYP_CHECK(ReadValue<ETextureDimension>(WireFields.at("dimension")) == State.Dimension);
	HYP_CHECK(ReadValue<std::string>(WireFields.at("pixelBytes")) == std::to_string(State.PixelBytes));
	const auto Decoded = ReadRecordWire(RecordType<FImportDraftInfo>(), Wire);
	const auto& RoundTrip = *static_cast<const FImportDraftInfo*>(Decoded.get());
	HYP_CHECK(RoundTrip.Dimension == State.Dimension && RoundTrip.PixelBytes == State.PixelBytes &&
	          RoundTrip.Width == State.Width && RoundTrip.Height == State.Height && RoundTrip.Details == State.Details);
	HYP_CHECK(InFixture.IO->Statistics().Writes.load() == Before);
	const auto OldGeneration = State.Generation;
	FImportPropertyEdits Edits;
	Edits.Name = "Draft texture";
	State = InFixture.Imports->EditDraft({State.Draft, State.Generation, Edits});
	HYP_CHECK(State.bDirty && State.Name == "Draft texture");
	Reject(
	    [&]
	    {
		    InFixture.Imports->EditDraft({State.Draft, OldGeneration, {}});
	    });
	Reject(
	    [&]
	    {
		    InFixture.Imports->DiscardDraft({State.Draft, State.Generation});
	    });
	Reject(
	    [&]
	    {
		    InFixture.Content->Clear({Request.Generation, false});
	    });
	State = InFixture.Imports->DraftHistory({State.Draft, State.Generation, "undo"});
	HYP_CHECK(State.Name == "Color" && !State.bDirty && State.bCanRedo);
	State = InFixture.Imports->DraftHistory({State.Draft, State.Generation, "redo"});
	HYP_CHECK(State.Name == "Draft texture");
	const auto Task = InFixture.Imports->SubmitDraft({State.Draft, State.Generation});
	HYP_CHECK(Task.Status == EImportTaskState::Running);
	HYP_CHECK(InFixture.Imports->Draft({State.Draft}).Status == EImportDraftState::Publishing);
	State = WaitDraft(InFixture, State.Draft);
	HYP_CHECK(State.Error.empty() && !State.bDirty);
	HYP_CHECK(InFixture.Imports->Get({Task.Task}).Status == EImportTaskState::Completed);
	const auto Texture = InFixture.Assets->LoadAsync<FTextureAsset>("/Game/Draft.hasset").Get(InFixture.Tasks);
	HYP_CHECK(Texture->Name == "Draft texture");
	const auto Header = DecodeAsset(InFixture.IO->ReadAsync("/Game/Draft.hasset").Get(InFixture.Tasks)).Header;
	HYP_CHECK(Header.Import && Header.Import->Settings.contains("property_overrides_v1"));
	const auto Repeat = InFixture.Imports->SubmitDraft({State.Draft, State.Generation});
	State = WaitDraft(InFixture, State.Draft);
	HYP_CHECK(InFixture.Imports->Get({Repeat.Task}).Result->bUpToDate);
	HYP_CHECK(InFixture.Imports->Get({Repeat.Task}).Result->WrittenAssets == 0);
	Edits.Nodes.push_back({"missing", "Wrong type", {}});
	Reject(
	    [&]
	    {
		    InFixture.Imports->EditDraft({State.Draft, State.Generation, Edits});
	    });
	HYP_CHECK(InFixture.Imports->Draft({State.Draft}).Generation == State.Generation);
	HYP_CHECK(InFixture.Imports->DiscardDraft({State.Draft, State.Generation}).Status == EImportDraftState::Discarded);
}

void CheckDraftFreshness(FFixture& InFixture)
{
	auto Request = InFixture.Request("Sky.hdr", "DraftSky.hasset");
	Request.Sky = FEnvironmentBakeSettings{8, 4, 8};
	Request.bForce = true;
	auto State = WaitDraft(InFixture, InFixture.Imports->PrepareDraft(Request).Draft);
	HYP_CHECK(State.Status == EImportDraftState::Ready && State.TotalProducts == 2);
	FImportPropertyEdits Edits;
	Edits.Name = "Prepared sky";
	State = InFixture.Imports->EditDraft({State.Draft, State.Generation, Edits});
	HYP_CHECK(State.bDirty && State.bCanUndo && !State.bCanRedo);
	const auto Original = InFixture.IO->ReadAsync(InFixture.Root / "Sky.hdr").Get(InFixture.Tasks);
	auto Changed = *Original;
	Changed.back() = std::byte{130};
	InFixture.IO->WriteAsync(InFixture.Root / "Sky.hdr", std::move(Changed)).Get(InFixture.Tasks);
	const auto Task = InFixture.Imports->SubmitDraft({State.Draft, State.Generation});
	State = WaitDraft(InFixture, State.Draft);
	HYP_CHECK(!State.Error.empty() && InFixture.Imports->Get({Task.Task}).Status == EImportTaskState::Failed);
	HYP_CHECK(State.Status == EImportDraftState::Ready && !InFixture.Imports->ContentRootState().bBusy);
	HYP_CHECK(State.bDirty && State.bCanUndo && !State.bCanRedo && State.Name == "Prepared sky");
	State = InFixture.Imports->DraftHistory({State.Draft, State.Generation, "undo"});
	HYP_CHECK(!State.bDirty && !State.bCanUndo && State.bCanRedo);
	State = InFixture.Imports->DraftHistory({State.Draft, State.Generation, "redo"});
	HYP_CHECK(State.bDirty && State.Name == "Prepared sky");
	const auto FailedGeneration = State.Generation;
	Edits.Name = "Retry sky";
	State = InFixture.Imports->EditDraft({State.Draft, State.Generation, Edits});
	HYP_CHECK(State.Generation > FailedGeneration && State.Status == EImportDraftState::Ready && State.Error.empty());
	HYP_CHECK(!InFixture.Files->Exists("/Game/DraftSky.hasset"));
	InFixture.IO->WriteAsync(InFixture.Root / "Sky.hdr", *Original).Get(InFixture.Tasks);
	InFixture.Imports->DiscardDraft({State.Draft, State.Generation, true});
	State = WaitDraft(InFixture, InFixture.Imports->PrepareDraft(Request).Draft);
	const auto Id = State.Draft;
	InFixture.Content->Clear({Request.Generation, false});
	Reject(
	    [&]
	    {
		    InFixture.Imports->Draft({Id});
	    });
	InFixture.Content->Change(InFixture.Root / "Game");
}

void CheckOutputValidation(FFixture& InFixture)
{
	auto Request = InFixture.Request("Color.png", "Check.hasset");
	Request.Output = PathToUtf8(InFixture.Root / "Game/Nested/Check.hasset");
	HYP_CHECK(InFixture.Imports->Validate(Request).Output == "/Game/Nested/Check.hasset");
	InFixture.Content->Clear({Request.Generation, false});
	Request.Generation = InFixture.Content->Info().Generation;
	Request.Output = PathToUtf8(InFixture.Root / "Outside.hasset");
	try
	{
		InFixture.Imports->ValidateOutput(Request);
		HYP_CHECK(false);
	}
	catch (const FAssetImportError& Failure)
	{
		HYP_CHECK(Failure.Code == "root_unset");
	}
	InFixture.Content->Change(InFixture.Root / "Game");
}

void CheckLifecycle(FFixture& InFixture)
{
	auto Request = InFixture.Request("Color.png", "Final.hasset");
	Request.Type = RecordType<FSkyAsset>().Id;
	Reject(
	    [&]
	    {
		    InFixture.Imports->Validate(Request);
	    });
	Request.Type.clear();
	Request.SourceId = "unpaired";
	Reject(
	    [&]
	    {
		    InFixture.Imports->Validate(Request);
	    });
	Request.SourceId.clear();
	const auto OldTask = InFixture.Wait(InFixture.Imports->Start(Request)).Task;
	InFixture.Content->Change(InFixture.Root / "Game", true);
	HYP_CHECK(InFixture.Imports->List().Total == 0);
	Reject(
	    [&]
	    {
		    InFixture.Imports->Get({OldTask});
	    });
	Request.Generation = InFixture.Content->Info().Generation;
	Reject(
	    [&]
	    {
		    InFixture.Imports->Start(Request);
	    });
	InFixture.Content->Change(InFixture.Root / "Game");
	Request.Generation = InFixture.Content->Info().Generation;
	const auto Task = InFixture.Imports->Start(Request);
	InFixture.Imports->Drain();
	HYP_CHECK(Task->Info.Status == EImportTaskState::Completed && !InFixture.Imports->ContentRootState().bBusy);
	Reject(
	    [&]
	    {
		    InFixture.Imports->Start(Request);
	    });
}
} // namespace

int main()
{
	try
	{
		FFixture Fixture;
		CheckImportStates();
		CheckContentPathContracts();
		CheckImportValidation(*Fixture.IO, *Fixture.Imports, Fixture.Request("Color.png", "Rules/Identity.hasset"));
		CheckImage(Fixture);
		CheckSky(Fixture);
		CheckFailedImport(Fixture);
		CheckSaveDiagnostics(Fixture);
		CheckContentFailureDiagnostic();
		CheckPanoramaDimensions(Fixture);
		CheckDraftMetadataSchema();
		CheckDraftPagination();
		CheckDraftEditing(Fixture);
		CheckDraftFreshness(Fixture);
		CheckOutputValidation(Fixture);
		CheckGroupedImports(Fixture);
		CheckGroupedDestinationEdges(Fixture);
		CheckFailedGroupCleanup(Fixture);
		CheckLifecycle(Fixture);
		std::cout << "Import workspace formats, freshness, identity and lifecycle passed\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
