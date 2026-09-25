#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/AssetImport/MaterialImport.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <thread>

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
		while (InTask->Info.Status == "running" && std::chrono::steady_clock::now() < End)
		{
			Imports->Update();
			Tasks.PumpMain();
			std::this_thread::yield();
		}
		if (InTask->Info.Status != "completed")
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
	auto Request = InFixture.Request("Sky.hdr", "Sky.hasset");
	Request.Sky = FEnvironmentBakeSettings{8, 4, 8};
	const auto First = InFixture.Wait(InFixture.Imports->Start(Request));
	const auto Again = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Again.bUpToDate && Again.WrittenAssets == 0);
	Request.Sky->RadianceSize = 16;
	const auto Changed = InFixture.Wait(InFixture.Imports->Start(Request));
	HYP_CHECK(Changed.Asset.Id == First.Asset.Id && !Changed.bUpToDate);
	const auto Sky = InFixture.Assets->LoadAsync<FSkyAsset>("/Game/Sky.hasset").Get(InFixture.Tasks);
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

void CheckFailedImport(FFixture& InFixture)
{
	InFixture.IO->WriteAsync(InFixture.Root / "Broken.png", FBytes{std::byte{0}}).Get(InFixture.Tasks);
	const auto Task = InFixture.Imports->Start(InFixture.Request("Broken.png", "Broken.hasset"));
	Reject(
	    [&]
	    {
		    InFixture.Wait(Task);
	    });
	HYP_CHECK(Task->Info.Status == "failed" && !Task->Info.Error.empty());
	HYP_CHECK(!InFixture.Imports->ContentRootState().bBusy);
	HYP_CHECK(!InFixture.Files->Exists("/Game/Broken.hasset"));
}

FImportDraftInfo WaitDraft(FFixture& InFixture, const std::string& InId)
{
	const auto End = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	for (;;)
	{
		InFixture.Imports->Update();
		InFixture.Tasks.PumpMain();
		auto State = InFixture.Imports->Draft({InId});
		if (State.Status != "preparing" && State.Status != "publishing")
		{
			return State;
		}
		HYP_CHECK(std::chrono::steady_clock::now() < End);
		std::this_thread::yield();
	}
}

void CheckDraftEditing(FFixture& InFixture)
{
	const auto Request = InFixture.Request("Color.png", "Draft.hasset");
	const auto Before = InFixture.IO->Statistics().Writes.load();
	auto State = InFixture.Imports->PrepareDraft(Request);
	HYP_CHECK(InFixture.Imports->ContentRootState().bBusy);
	State = WaitDraft(InFixture, State.Draft);
	HYP_CHECK(State.Status == "ready" && State.Width == 2 && State.Mips == 2);
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
	State = WaitDraft(InFixture, State.Draft);
	HYP_CHECK(State.Error.empty() && !State.bDirty);
	HYP_CHECK(InFixture.Imports->Get({Task.Task}).Status == "completed");
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
	InFixture.Imports->DiscardDraft({State.Draft, State.Generation});
}

void CheckDraftFreshness(FFixture& InFixture)
{
	auto Request = InFixture.Request("Sky.hdr", "DraftSky.hasset");
	Request.Sky = FEnvironmentBakeSettings{8, 4, 8};
	Request.bForce = true;
	auto State = WaitDraft(InFixture, InFixture.Imports->PrepareDraft(Request).Draft);
	HYP_CHECK(State.Status == "ready" && State.TotalProducts == 2);
	const auto Original = InFixture.IO->ReadAsync(InFixture.Root / "Sky.hdr").Get(InFixture.Tasks);
	auto Changed = *Original;
	Changed.back() = std::byte{130};
	InFixture.IO->WriteAsync(InFixture.Root / "Sky.hdr", std::move(Changed)).Get(InFixture.Tasks);
	const auto Task = InFixture.Imports->SubmitDraft({State.Draft, State.Generation});
	State = WaitDraft(InFixture, State.Draft);
	HYP_CHECK(!State.Error.empty() && InFixture.Imports->Get({Task.Task}).Status == "failed");
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
	HYP_CHECK(Task->Info.Status == "completed" && !InFixture.Imports->ContentRootState().bBusy);
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
		CheckImage(Fixture);
		CheckSky(Fixture);
		CheckFailedImport(Fixture);
		CheckDraftEditing(Fixture);
		CheckDraftFreshness(Fixture);
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
