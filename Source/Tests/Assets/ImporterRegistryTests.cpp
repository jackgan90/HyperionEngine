#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/AssetImport/ImporterRegistry.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/Scene/ModelSource.h"
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

FAssetImporter TestImporter()
{
	return {"test.texture-import",
	        1,
	        &RecordType<FTextureAsset>(),
	        {".TEST_TEXTURE"},
	        [Texture = std::make_shared<FTextureAsset>(
	             BuildTextureAsset("registry-test", EMaterialTextureEncoding::Linear, {1, 1, {255, 0, 0, 255}}))](
	            FAssetImportContext& InContext) -> std::shared_ptr<void>
	        {
		        HYP_CHECK(InContext.Bytes && InContext.Bytes->size() == 1);
		        return Texture;
	        },
	        EAssetImporterExposure::Workspace,
	        "Test-only texture source.",
	        {true, false}};
}

void Registration()
{
	FTaskSystem Tasks(2, 1);
	FIOService IO(Tasks);
	FAssetImportService Service(IO);
	for (const auto* Extension : {"test", ".", ".a/b", ".a.png", ".HAsset"})
	{
		auto Importer = TestImporter();
		Importer.Extensions = {Extension};
		Reject(
		    [&]
		    {
			    Service.Register(Importer);
		    });
		HYP_CHECK(Service.ImporterDescriptors().empty());
	}
	auto Duplicate = TestImporter();
	Duplicate.Extensions = {".SAME", ".same"};
	Reject(
	    [&]
	    {
		    Service.Register(Duplicate);
	    });
	{
		auto LocalType = RecordType<FTextureAsset>();
		auto Importer = TestImporter();
		Importer.Type = &LocalType;
		Service.Register(std::move(Importer));
	}
	const auto Descriptors = Service.ImporterDescriptors();
	HYP_CHECK(Descriptors.size() == 1 && Descriptors[0].Extensions == std::vector<std::string>{".test_texture"});
	HYP_CHECK(Descriptors[0].OwnedType && Descriptors[0].Type == Descriptors[0].OwnedType.get());
	HYP_CHECK(Descriptors[0].Type->Definition == RecordType<FTextureAsset>().Definition);
	HYP_CHECK(ProjectImportCapabilities(Descriptors).Formats[0].Extensions == Descriptors[0].Extensions);
	Duplicate = TestImporter();
	Duplicate.Extensions = {".other"};
	Reject(
	    [&]
	    {
		    Service.Register(Duplicate);
	    });
	Duplicate.Id = "test.other";
	Duplicate.Extensions = {".test_texture"};
	Reject(
	    [&]
	    {
		    Service.Register(Duplicate);
	    });
	auto ConflictingType = RecordType<FTextureAsset>();
	ConflictingType.Id = "test.conflicting-texture";
	Duplicate.Type = &ConflictingType;
	Duplicate.Extensions = {".other"};
	Reject(
	    [&]
	    {
		    Service.Register(Duplicate);
	    });
	HYP_CHECK(Service.ImporterDescriptors().size() == 1);
	Service.FreezeImporters();
	Duplicate = TestImporter();
	Duplicate.Id = "test.late";
	Duplicate.Extensions = {".late"};
	Reject(
	    [&]
	    {
		    Service.Register(Duplicate);
	    });
	HYP_CHECK(Service.ImporterDescriptors().size() == 1);
}

void DefaultProjection()
{
	const auto Descriptors = DefaultAssetImporters();
	const auto Capabilities = FAssetImportWorkspace::Capabilities();
	HYP_CHECK(Capabilities.Formats.size() == 3 && !Capabilities.bCancellable);
	HYP_CHECK(Capabilities.Formats[0].Type == RecordType<FModelAsset>().Id);
	HYP_CHECK(Capabilities.Formats[0].Extensions == std::vector<std::string>({".gltf", ".glb"}));
	HYP_CHECK(Capabilities.Formats[1].Type == RecordType<FTextureAsset>().Id);
	HYP_CHECK(Capabilities.Formats[1].Extensions == std::vector<std::string>({".png", ".jpg", ".jpeg"}));
	HYP_CHECK(Capabilities.Formats[2].Type == RecordType<FSkyAsset>().Id);
	HYP_CHECK(Capabilities.Formats[2].Extensions == std::vector<std::string>({".hdr", ".exr"}));
	const auto* Tooling = FindAssetImporter(Descriptors, ".gltf", RecordType<FModelSource>().Id, true);
	HYP_CHECK(Tooling && Tooling->Exposure == EAssetImporterExposure::ToolingOnly);
	const std::vector<FAssetImporter> ToolingOnly{*Tooling};
	HYP_CHECK(ProjectImportCapabilities(ToolingOnly).Formats.empty());
	HYP_CHECK(!FindAssetImporter(ToolingOnly, ".gltf", {}, true));
	HYP_CHECK(FindAssetImporter(ToolingOnly, ".gltf", RecordType<FModelSource>().Id, true));
}

void RequestedNames(FTaskSystem& InTasks, FAssetService& InAssets, FAssetImportWorkspace& InWorkspace,
                    FImportRequest InRequest)
{
	InRequest.Name = "RequestedTextureName";
	InRequest.Output = "/Game/Named.hasset";
	const auto Direct = InWorkspace.Start(InRequest);
	const auto End = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (Direct->Info.Status == EImportTaskState::Running && std::chrono::steady_clock::now() < End)
	{
		InTasks.PumpMain();
		InWorkspace.Update();
		std::this_thread::yield();
	}
	HYP_CHECK(Direct->Info.Status == EImportTaskState::Completed);
	const auto DirectTexture = InAssets.LoadAsync<FTextureAsset>(InRequest.Output).Get(InTasks);
	InRequest.Output = "/Game/DraftNamed.hasset";
	auto Draft = InWorkspace.PrepareDraft(InRequest);
	while (Draft.Status == EImportDraftState::Preparing && std::chrono::steady_clock::now() < End)
	{
		InTasks.PumpMain();
		InWorkspace.Update();
		std::this_thread::yield();
		Draft = InWorkspace.Draft({Draft.Draft});
	}
	HYP_CHECK(Draft.Status == EImportDraftState::Ready);
	const auto PreparedName = Draft.Name;
	auto Published = InWorkspace.SubmitDraft({Draft.Draft, Draft.Generation});
	while (Published.Status == EImportTaskState::Running && std::chrono::steady_clock::now() < End)
	{
		InTasks.PumpMain();
		InWorkspace.Update();
		std::this_thread::yield();
		Published = InWorkspace.Get({Published.Task});
	}
	HYP_CHECK(Published.Status == EImportTaskState::Completed);
	const auto PreparedTexture = InAssets.LoadAsync<FTextureAsset>(InRequest.Output).Get(InTasks);
	std::cout << "Custom texture names: direct=" << DirectTexture->Name << ", draft=" << PreparedName
	          << ", prepared publication=" << PreparedTexture->Name << '\n';
	HYP_CHECK(DirectTexture->Name == InRequest.Name);
	HYP_CHECK(PreparedName == InRequest.Name && PreparedTexture->Name == InRequest.Name);
	InRequest.Name.clear();
	InRequest.Output = "/Game/CachedDefault.hasset";
	const auto Unnamed = InWorkspace.Start(InRequest);
	while (Unnamed->Info.Status == EImportTaskState::Running && std::chrono::steady_clock::now() < End)
	{
		InTasks.PumpMain();
		InWorkspace.Update();
		std::this_thread::yield();
	}
	HYP_CHECK(Unnamed->Info.Status == EImportTaskState::Completed);
	HYP_CHECK(InAssets.LoadAsync<FTextureAsset>(InRequest.Output).Get(InTasks)->Name == "registry-test");
}

void SelectedWorkspace()
{
	FTaskSystem Tasks(2, 1);
	const auto Root = std::filesystem::current_path() / "importer-registry" / CreateIdentifier();
	std::filesystem::create_directories(Root / "Engine");
	std::filesystem::create_directories(Root / "Game");
	auto Files = CreateContentFileSystem(Root / "Engine", true);
	FIOService IO(Tasks, Files);
	FAssetService Assets(IO);
	RegisterSceneAssetTypes(Assets.Types());
	FContentRootService Content(Tasks, *Files, Assets);
	auto Tooling = TestImporter();
	Tooling.Id = "test.tooling-first";
	Tooling.Type = &RecordType<FModelSource>();
	Tooling.Exposure = EAssetImporterExposure::ToolingOnly;
	Tooling.Settings = {};
	Tooling.Convert = [](FAssetImportContext&) -> std::shared_ptr<void>
	{
		throw std::runtime_error("Workspace must preserve its selected importer");
	};
	FAssetImportWorkspace Workspace(IO, Assets, Content, {std::move(Tooling), TestImporter()});
	Content.RegisterParticipant(Workspace);
	Content.Change(Root / "Game");
	const auto Source = Root / "color.TEST_TEXTURE";
	IO.WriteAsync(Source, {std::byte{1}}).Get(Tasks);
	const auto Capabilities = Workspace.GetCapabilities();
	HYP_CHECK(Capabilities.Formats.size() == 1 && Capabilities.Formats[0].Type == RecordType<FTextureAsset>().Id);
	HYP_CHECK(Capabilities.Formats[0].Extensions == std::vector<std::string>{".test_texture"});
	FImportRequest Request;
	Request.Generation = Content.Info().Generation;
	Request.Source = PathToUtf8(Source);
	Request.Output = "/Game/Color.hasset";
	Request.TextureEncoding = EMaterialTextureEncoding::Linear;
	const auto Selection = Workspace.SelectSource(Request.Source);
	HYP_CHECK(Selection.Type == RecordType<FTextureAsset>().Id && Selection.Settings.bTextureEncoding);
	const auto Validation = Workspace.Validate(Request);
	HYP_CHECK(Validation.Type == RecordType<FTextureAsset>().Id);
	const auto Task = Workspace.Start(Request);
	while (Task->Info.Status == EImportTaskState::Running)
	{
		Tasks.PumpMain();
		Workspace.Update();
	}
	HYP_CHECK(Task->Info.Status == EImportTaskState::Completed && Task->Info.Result);
	const auto Header = Assets.LoadAsync<FTextureAsset>("/Game/Color.hasset").Get(Tasks);
	HYP_CHECK(Header->Name == "registry-test" && Header->Mips.front().Width == 1 && Header->Mips.front().Height == 1);
	RequestedNames(Tasks, Assets, Workspace, Request);
	auto Rejected = Request;
	Rejected.Type = RecordType<FModelAsset>().Id;
	Reject(
	    [&]
	    {
		    (void)Workspace.Validate(Rejected);
	    });
	Rejected = Request;
	Rejected.Output = "/Engine/No.hasset";
	Reject(
	    [&]
	    {
		    (void)Workspace.Validate(Rejected);
	    });
	Rejected = Request;
	++Rejected.Generation;
	Reject(
	    [&]
	    {
		    (void)Workspace.Validate(Rejected);
	    });
	Content.UnregisterParticipant(Workspace);
	Workspace.Drain();
}
} // namespace

void CheckImporterRegistry()
{
	Registration();
	DefaultProjection();
	SelectedWorkspace();
}
