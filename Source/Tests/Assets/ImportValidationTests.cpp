#include "Hyperion/AssetImport/ImageImport.h"
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

struct FValidationFixture
{
	FIOService& IO;
	FAssetImportWorkspace& Workspace;
	FImportRequest Request;
	FAssetImportService Service{IO};

	FValidationFixture(FIOService& InIO, FAssetImportWorkspace& InWorkspace, FImportRequest InRequest)
	    : IO(InIO), Workspace(InWorkspace), Request(std::move(InRequest))
	{
		RegisterImageImporter(Service);
	}
};

template<class TFunction> void CheckWorkspaceError(TFunction InAction, std::string_view InMessage)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const FAssetImportError& Failure)
	{
		bRejected = Failure.Code == AssetImportErrors::InvalidArguments && Failure.what() == InMessage;
	}
	HYP_CHECK(bRejected);
}

template<class TFunction> void CheckServiceError(TFunction InAction, std::string_view InMessage)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::invalid_argument& Failure)
	{
		bRejected = Failure.what() == InMessage;
	}
	HYP_CHECK(bRejected);
}

void CheckSourceIdentity(FValidationFixture& InFixture)
{
	struct FCase
	{
		bool bHasRoot;
		std::string Id;
		std::string ServiceError;
	};

	const FCase Cases[]{{true, "", "Source root and source ID must be supplied together"},
	                    {false, "image", "Source root and source ID must be supplied together"},
	                    {false, "bad:id", "Source root and source ID must be supplied together"},
	                    {true, "bad:id", "Source ID must be a portable logical name"},
	                    {true, "bad\\id", "Source ID must be a portable logical name"},
	                    {true, "/image", "Source ID must be a portable logical name"},
	                    {true, "../image", "Source ID must be a portable logical name"},
	                    {true, "image..suffix", "Source ID must be a portable logical name"}};
	const auto Before = InFixture.IO.Statistics().Writes.load();
	for (const auto& Case : Cases)
	{
		auto Request = InFixture.Request;
		Request.SourceRoot = Case.bHasRoot ? PathToUtf8(PathFromUtf8(Request.Source).parent_path()) : "";
		Request.SourceId = Case.Id;
		CheckWorkspaceError(
		    [&]
		    {
			    (void)InFixture.Workspace.Validate(Request);
		    },
		    "Supply sourceRoot and a portable sourceId together");
		FAssetImportOptions Options;
		Options.SourceRoot = PathFromUtf8(Request.SourceRoot);
		Options.SourceId = Case.Id;
		// Acceptance is synchronous; identity rejection must still come from the worker result->
		const auto Pending =
		    InFixture.Service.ImportAsync(PathFromUtf8(Request.Source), PathFromUtf8(Request.Output), Options);
		CheckServiceError(
		    [&]
		    {
			    (void)Pending.Get(InFixture.IO.TaskSystem());
		    },
		    Case.ServiceError);
		HYP_CHECK(InFixture.IO.Statistics().Writes.load() == Before);
		HYP_CHECK(!InFixture.IO.FileSystem()->Exists(PathFromUtf8(Request.Output)));
	}
	for (const auto& Id : {"", "images/color", "images/color.v1", "颜色/image"})
	{
		auto Request = InFixture.Request;
		Request.SourceId = Id;
		Request.SourceRoot = Request.SourceId.empty() ? "" : PathToUtf8(PathFromUtf8(Request.Source).parent_path());
		(void)InFixture.Workspace.Validate(Request);
		FAssetImportOptions Options;
		Options.SourceRoot = PathFromUtf8(Request.SourceRoot);
		Options.SourceId = Request.SourceId;
		const auto Result =
		    InFixture.Service.ImportAsync(PathFromUtf8(Request.Source), PathFromUtf8(Request.Output), Options)
		        .Get(InFixture.IO.TaskSystem());
		HYP_CHECK(Result->Header.Import.has_value());
	}
}

void CheckOutputRules(FValidationFixture& InFixture)
{
	const auto Before = InFixture.IO.Statistics().Writes.load();
	auto Request = InFixture.Request;
	Request.Output = "/Game/Rules/Bad.png";
	CheckWorkspaceError(
	    [&]
	    {
		    InFixture.Workspace.ValidateOutput(Request);
	    },
	    "Output filename must end with .hasset");
	CheckServiceError(
	    [&]
	    {
		    (void)InFixture.Service.ImportAsync(PathFromUtf8(Request.Source), PathFromUtf8(Request.Output));
	    },
	    "Import output must be a separate .hasset file");
	Request.Source = PathToUtf8(PathFromUtf8(InFixture.Request.Source).parent_path() / "Game/Rules/Same.hasset");
	Request.Output = "/Game/Rules/Sub/../Same.hasset";
	CheckWorkspaceError(
	    [&]
	    {
		    (void)InFixture.Workspace.Validate(Request);
	    },
	    "Output must be a separate .hasset file");
	CheckServiceError(
	    [&]
	    {
		    (void)InFixture.Service.ImportAsync(PathFromUtf8(Request.Source), PathFromUtf8(Request.Output));
	    },
	    "Import output must be a separate .hasset file");
	Request = InFixture.Request;
	Request.Output = "/Engine/Rules/Asset.hasset";
	CheckWorkspaceError(
	    [&]
	    {
		    InFixture.Workspace.ValidateOutput(Request);
	    },
	    "Save the asset and its dependencies inside the current /Game root");
	Request = InFixture.Request;
	Request.Library = "/Engine/Rules";
	CheckWorkspaceError(
	    [&]
	    {
		    InFixture.Workspace.ValidateOutput(Request);
	    },
	    "Save the asset and its dependencies inside the current /Game root");
	Request = InFixture.Request;
	for (const auto& Id : {std::string(4097, 'x'), std::string("a\0b", 3)})
	{
		Request.SourceId = Id;
		CheckWorkspaceError(
		    [&]
		    {
			    (void)InFixture.Workspace.Validate(Request);
		    },
		    "Import strings must be at most 4096 bytes without NUL");
	}
	HYP_CHECK(InFixture.IO.Statistics().Writes.load() == Before);
}

void CheckLibraryRules(FValidationFixture& InFixture)
{
	auto Request = InFixture.Request;
	Request.Output = "/Game/Rules/Sub/../Library.HASSET";
	Request.Library.clear();
	const auto Before = InFixture.IO.Statistics().Writes.load();
	const auto Default = InFixture.Workspace.Validate(Request);
	HYP_CHECK(Default.Output == "/Game/Rules/Library.HASSET" && Default.Library == "/Game/Rules");
	Request.Library = "/Game/Rules/Sub/..";
	const auto Explicit = InFixture.Workspace.Validate(Request);
	HYP_CHECK(Default.Output == Explicit.Output && Default.Library == Explicit.Library);
	const auto Source = PathFromUtf8(Request.Source);
	const auto Output = PathFromUtf8(Request.Output);
	FAssetImportOptions Options;
	Options.RootId = "0123456789abcdef0123456789abcdef";
	const auto Prepared = InFixture.Service.PrepareAsync(Source, Output, Options).Get(InFixture.IO.TaskSystem());
	Options.Library = PathFromUtf8(Request.Library);
	const auto Again = InFixture.Service.PrepareAsync(Source, Output, Options).Get(InFixture.IO.TaskSystem());
	HYP_CHECK(Prepared->Root.Type->Id == Again->Root.Type->Id && Prepared->Sources == Again->Sources);
	HYP_CHECK(InFixture.IO.Statistics().Writes.load() == Before);
	Options.Library.clear();
	const auto First = InFixture.Service.ImportAsync(Source, Output, Options).Get(InFixture.IO.TaskSystem());
	HYP_CHECK(First->Header.Import->Settings.at("library") == Default.Library);
	Options.Library = PathFromUtf8(Request.Library);
	const auto Reimport = InFixture.Service.ImportAsync(Source, Output, Options).Get(InFixture.IO.TaskSystem());
	HYP_CHECK(Reimport->Header.Id == First->Header.Id && Reimport->bUpToDate && Reimport->WrittenAssets == 0);
	Request.Library = "/Game/Rules/../SeparateLibrary";
	HYP_CHECK(InFixture.Workspace.Validate(Request).Library == "/Game/SeparateLibrary");
	Options.Library = PathFromUtf8(Request.Library);
	const auto Separate = InFixture.Service.ImportAsync(Source, Output, Options).Get(InFixture.IO.TaskSystem());
	HYP_CHECK(Separate->Header.Import->Settings.at("library") == "/Game/SeparateLibrary");
}

void CheckGroupedRules(FValidationFixture& InFixture)
{
	auto Request = InFixture.Request;
	Request.Output = "/Game/Rules/Grouped/Asset.hasset";
	Request.Library = "/Game/Rules/Grouped";
	Request.bCreateFolder = true;
	const auto Before = InFixture.IO.Statistics().Writes.load();
	CheckWorkspaceError(
	    [&]
	    {
		    (void)InFixture.Workspace.Validate(Request);
	    },
	    "Grouped imports cannot specify a dependency library");
	FAssetImportOptions Options;
	Options.bCreateFolder = true;
	Options.Library = "/Game/SeparateLibrary";
	CheckServiceError(
	    [&]
	    {
		    (void)InFixture.Service.ImportAsync(PathFromUtf8(Request.Source), PathFromUtf8(Request.Output), Options);
	    },
	    "Grouped imports cannot use a separate dependency library");
	HYP_CHECK(InFixture.IO.Statistics().Writes.load() == Before);
	Options.Library = PathFromUtf8(Request.Library) / ".";
	Request.Library.clear();
	const auto Validated = InFixture.Workspace.Validate(Request);
	const auto Result =
	    InFixture.Service.ImportAsync(PathFromUtf8(Request.Source), PathFromUtf8(Request.Output), Options)
	        .Get(InFixture.IO.TaskSystem());
	HYP_CHECK(PathToUtf8(Result->Output.parent_path()) == Validated.Folder && Result->WrittenAssets == 1);
}

void CheckPreparationAdmission(FValidationFixture& InFixture)
{
	const auto Source = PathFromUtf8(InFixture.Request.Source);
	const std::filesystem::path PreviewOutput = "/Game/Rules/Preparation.preview";
	const std::filesystem::path NativeOutput = "/Game/Rules/Preparation.hasset";
	const auto Before = InFixture.IO.Statistics().Writes.load();
	const auto Preview = InFixture.Service.PrepareAsync(Source, PreviewOutput).Get(InFixture.IO.TaskSystem());
	HYP_CHECK(Preview->Root.Type->Id == RecordType<FTextureAsset>().Id && Preview->Root.Object);
	CheckServiceError(
	    [&]
	    {
		    (void)InFixture.Service.ImportAsync(Source, PreviewOutput);
	    },
	    "Import output must be a separate .hasset file");

	FAssetImportOptions Options;
	Options.SourceRoot = Source.parent_path();
	const auto Prepared = InFixture.Service.PrepareAsync(Source, NativeOutput, Options).Get(InFixture.IO.TaskSystem());
	HYP_CHECK(Prepared->Root.Type->Id == Preview->Root.Type->Id && Prepared->Sources == Preview->Sources);
	// Read-only preparation fills the omitted ID internally; publication retains its stricter admission.
	const auto Publication = InFixture.Service.ImportAsync(Source, NativeOutput, Options);
	CheckServiceError(
	    [&]
	    {
		    (void)Publication.Get(InFixture.IO.TaskSystem());
	    },
	    "Source root and source ID must be supplied together");
	HYP_CHECK(InFixture.IO.Statistics().Writes.load() == Before);
	HYP_CHECK(!InFixture.IO.FileSystem()->Exists(PreviewOutput));
	HYP_CHECK(!InFixture.IO.FileSystem()->Exists(NativeOutput));
}
} // namespace

void CheckImportValidation(Hyperion::FIOService& InIO, Hyperion::FAssetImportWorkspace& InWorkspace,
                           Hyperion::FImportRequest InRequest)
{
	FValidationFixture Fixture(InIO, InWorkspace, std::move(InRequest));
	CheckOutputRules(Fixture);
	CheckLibraryRules(Fixture);
	CheckGroupedRules(Fixture);
	CheckSourceIdentity(Fixture);
	CheckPreparationAdmission(Fixture);
}
