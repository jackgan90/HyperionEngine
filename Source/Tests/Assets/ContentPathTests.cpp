#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Content/ContentQueries.h"
#include "Hyperion/Core/ContentHash.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

struct FContentPathCase
{
	std::string_view Path;
	bool bGame{};
	bool bEngine{};
};

struct FContentDirectoryErrorCase
{
	FContentDirectoryQuery Query;
	std::string_view Message;
};

template<class TError, class TAction>
void CheckRootError(TAction InAction, std::string_view InCode, std::string_view InMessage)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const TError& Failure)
	{
		bRejected = Failure.Code == InCode && Failure.what() == InMessage;
	}
	HYP_CHECK(bRejected);
}

void CheckRootMembership()
{
	const FContentPathCase Cases[]{{"/Game", true},
	                               {"/Game/", true},
	                               {"/Game/Textures/颜色.hasset", true},
	                               {"/Engine", false, true},
	                               {"/Engine/Texture.hasset", false, true},
	                               {"/Gameplay/Asset.hasset"},
	                               {"/EngineExtra"},
	                               {"/game/Asset.hasset"},
	                               {"Game/Asset.hasset"},
	                               {"F:/Game/Asset.hasset"},
	                               {"/Game\\Asset.hasset"},
	                               {""},
	                               {"/Game/../Engine", true},
	                               {"/Game//Nested", true},
	                               {"/Game/./Asset.hasset", true}};
	for (const auto& Case : Cases)
	{
		HYP_CHECK(IsGameContentPath(Case.Path) == Case.bGame);
		HYP_CHECK(IsEngineContentPath(Case.Path) == Case.bEngine);
	}
	HYP_CHECK(std::string_view(GameContentRoot) == "/Game");
	HYP_CHECK(std::string_view(EngineContentRoot) == "/Engine");
	HYP_CHECK(FContentDirectoryQuery{}.Directory == "/Game");
}

void CheckDirectoryErrors(FAssetService& InAssets, FContentRootService& InRoots)
{
	for (const auto* Path : {"/Game", "/Game/", "/Game/Texture.hasset"})
	{
		CheckRootError<FContentRootError>(
		    [&]
		    {
			    (void)QueryContentDirectory(InAssets, InRoots, {0, Path});
		    },
		    "root_unset", "Select Game content with content.root.set");
	}
	CheckRootError<FContentRootError>(
	    [&]
	    {
		    (void)QueryContentDirectory(InAssets, InRoots, {1, "/Game/../Engine", 0, 0});
	    },
	    "stale_revision", "Content root changed; query content.root.get");

	const FContentDirectoryErrorCase Cases[]{
	    {{0, "/Game", 0, 0}, "Directory page limit must be 1-100"},
	    {{0, "/Game/../Engine"}, "Parent traversal is not allowed"},
	    {{0, "/Game/Folder/.."}, "Parent traversal is not allowed"},
	    {{0, "/Game\\Folder"}, "Directory must be an absolute /Game or /Engine package path"},
	    {{0, "/Gameplay"}, "Directory must be an absolute /Game or /Engine package path"},
	    {{0, "/game"}, "Directory must be an absolute /Game or /Engine package path"},
	    {{0, "F:/Game/Folder"}, "Directory must be an absolute /Game or /Engine package path"}};
	for (const auto& Case : Cases)
	{
		bool bRejected{};
		try
		{
			(void)QueryContentDirectory(InAssets, InRoots, Case.Query);
		}
		catch (const std::invalid_argument& Failure)
		{
			bRejected = Failure.what() == Case.Message;
		}
		HYP_CHECK(bRejected);
	}
	HYP_CHECK(QueryContentDirectory(InAssets, InRoots, {0, "/Engine"}).Entries.empty());
}

void CheckImportRootErrors(FAssetImportWorkspace& InImports, FContentRootService& InRoots,
                           const std::filesystem::path& InGame)
{
	FImportRequest Request;
	Request.Generation = 1;
	CheckRootError<FAssetImportError>(
	    [&]
	    {
		    InImports.ValidateOutput(Request);
	    },
	    "stale_revision", "Content root changed before import");
	Request.Generation = 0;
	CheckRootError<FAssetImportError>(
	    [&]
	    {
		    InImports.ValidateOutput(Request);
	    },
	    "root_unset", "Select a Game asset root through File > Open before choosing an output");
	HYP_CHECK(InImports.List().Total == 0);
	InRoots.Change(InGame, true);
	Request.Generation = InRoots.Info().Generation;
	CheckRootError<FAssetImportError>(
	    [&]
	    {
		    InImports.ValidateOutput(Request);
	    },
	    "read_only", "Game content is read-only");
	HYP_CHECK(InImports.List().Total == 0);
}
} // namespace

void CheckContentPathContracts()
{
	using namespace Hyperion;
	CheckRootMembership();
	const auto Root = std::filesystem::absolute("content-path-contracts") / CreateIdentifier();
	std::filesystem::create_directories(Root / "Engine");
	std::filesystem::create_directories(Root / "Game");
	FTaskSystem Tasks{1, 1};
	auto Files = CreateContentFileSystem(Root / "Engine");
	FIOService IO{Tasks, Files};
	FAssetService Assets{IO};
	FContentRootService Roots{Tasks, *Files, Assets};
	FAssetImportWorkspace Imports{IO, Assets, Roots};
	Roots.RegisterParticipant(Imports);
	const auto Writes = IO.Statistics().Writes.load();
	CheckDirectoryErrors(Assets, Roots);
	HYP_CHECK(Roots.Info().Generation == 0 && Roots.Info().Directory.empty());
	CheckImportRootErrors(Imports, Roots, Root / "Game");
	HYP_CHECK(IO.Statistics().Writes.load() == Writes && Assets.GetAssetIndex().empty());
	Roots.UnregisterParticipant(Imports);
}
