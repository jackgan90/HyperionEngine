#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Reflection/Wire.h"
#include "Support/TestSupport.h"
#include <thread>

namespace
{
using namespace Hyperion;

struct FHistoryStep
{
	EImportDraftHistoryAction Action;
	const char* Token;
	const char* Name;
	bool bDirty;
	bool bCanUndo;
	bool bCanRedo;
};

std::string Snapshot(const FImportDraftInfo& InState)
{
	return WriteJson(WriteRecordWire(RecordType<FImportDraftInfo>(), &InState));
}

template<class TAction> void CheckHistoryError(TAction InAction, std::string_view InCode, std::string_view InMessage)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const FAssetImportError& Failure)
	{
		bRejected = Failure.Code == InCode && Failure.what() == InMessage;
	}
	HYP_CHECK(bRejected);
}

void CheckHistoryWire()
{
	const auto& Type = RecordType<FImportDraftHistory>();
	const std::vector<std::string> Tokens{"reset", "undo",    "redo",  "",
	                                      "UNDO",  "unknown", " undo", std::string("undo\0suffix", 11)};
	for (const auto& Token : Tokens)
	{
		auto Wire = ParseJson(R"({"draft":"draft-id","generation":"7","action":""})");
		std::get<FArchiveNode::FObject>(Wire.Value).at("action") = WriteValue(Token);
		const auto Decoded = ReadRecordWire(Type, Wire);
		const auto& Request = *static_cast<const FImportDraftHistory*>(Decoded.get());
		HYP_CHECK(Request.Draft == "draft-id" && Request.Generation == 7 && Request.Action == Token);
		HYP_CHECK(WriteJson(WriteRecordWire(Type, &Request)) == WriteJson(Wire));
		auto Archive = ParseJson(
		    R"({"type":"asset.import.draft-history","version":1,"fields":{"draft":"draft-id","generation":7,"action":""}})");
		auto& Fields =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Archive.Value).at("fields").Value);
		Fields.at("action") = WriteValue(Token);
		const auto Restored = ReadRecord(Type, Archive);
		HYP_CHECK(static_cast<const FImportDraftHistory*>(Restored.get())->Action == Token);
		HYP_CHECK(WriteJson(WriteRecord(Type, Restored.get())) == WriteJson(Archive));
	}
	const auto Default = ReadRecordWire(Type, ParseJson(R"({"draft":"draft-id","generation":"7"})"));
	HYP_CHECK(static_cast<const FImportDraftHistory*>(Default.get())->Action.empty());
	const auto Schema = RecordWireSchema(Type);
	const auto& Properties =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Schema.Value).at("properties").Value);
	HYP_CHECK(WriteJson(Properties.at("action")) == WriteJson(ParseJson(R"({"type":"string","default":""})")));
	HYP_CHECK(ResolveRecordMember(Type, &FImportDraftHistory::Action).FieldId == "action");
}

FImportDraftInfo WaitDraft(FTaskSystem& InTasks, FAssetImportWorkspace& InImports, FImportDraftInfo InState)
{
	const auto End = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (InState.Status == EImportDraftState::Preparing && std::chrono::steady_clock::now() < End)
	{
		InImports.Update();
		InTasks.PumpMain();
		std::this_thread::yield();
		InState = InImports.Draft({InState.Draft});
	}
	HYP_CHECK(InState.Status == EImportDraftState::Ready);
	return InState;
}

void CheckRejectedHistory(FAssetImportWorkspace& InImports, const FImportDraftInfo& InState)
{
	const auto Before = Snapshot(InState);
	for (const auto* Action : {"", "unknown", "UNDO", " undo", "undo", "redo"})
	{
		CheckHistoryError(
		    [&]
		    {
			    (void)InImports.DraftHistory({InState.Draft, InState.Generation, Action});
		    },
		    "invalid_arguments", "History requires available undo, redo or reset");
		HYP_CHECK(Snapshot(InImports.Draft({InState.Draft})) == Before);
	}
	CheckHistoryError(
	    [&]
	    {
		    (void)InImports.DraftHistory({InState.Draft, InState.Generation - 1, "unknown"});
	    },
	    "stale_revision", "Import draft changed before the operation");
	CheckHistoryError(
	    [&]
	    {
		    (void)InImports.DraftHistory({"missing", 0, "unknown"});
	    },
	    "not_found", "Import draft was discarded or belongs to another root");
	for (const auto Action : {EImportDraftHistoryAction::Invalid, static_cast<EImportDraftHistoryAction>(999),
	                          EImportDraftHistoryAction::Undo, EImportDraftHistoryAction::Redo})
	{
		CheckHistoryError(
		    [&]
		    {
			    (void)InImports.DraftHistory({InState.Draft, InState.Generation}, Action);
		    },
		    "invalid_arguments", "History requires available undo, redo or reset");
	}
	HYP_CHECK(Snapshot(InImports.Draft({InState.Draft})) == Before);
}

void CheckHistoryParity(FAssetImportWorkspace& InImports, FImportDraftInfo InTyped, FImportDraftInfo InProtocol)
{
	FImportPropertyEdits Edits;
	Edits.Name = "History edit";
	InTyped = InImports.EditDraft({InTyped.Draft, InTyped.Generation, Edits});
	InProtocol = InImports.EditDraft({InProtocol.Draft, InProtocol.Generation, Edits});
	const FHistoryStep Steps[]{{EImportDraftHistoryAction::Undo, "undo", "Color", false, false, true},
	                           {EImportDraftHistoryAction::Redo, "redo", "History edit", true, true, false},
	                           {EImportDraftHistoryAction::Reset, "reset", "Color", false, true, false},
	                           {EImportDraftHistoryAction::Undo, "undo", "History edit", true, true, true}};
	for (const auto& Step : Steps)
	{
		const auto Generation = InTyped.Generation;
		InTyped = InImports.DraftHistory({InTyped.Draft, InTyped.Generation}, Step.Action);
		InProtocol = InImports.DraftHistory({InProtocol.Draft, InProtocol.Generation, Step.Token});
		HYP_CHECK(InTyped.Generation == Generation + 1 && InTyped.Name == Step.Name);
		HYP_CHECK(InTyped.bDirty == Step.bDirty && InTyped.bCanUndo == Step.bCanUndo &&
		          InTyped.bCanRedo == Step.bCanRedo);
		auto Comparable = InProtocol;
		Comparable.Draft = InTyped.Draft;
		HYP_CHECK(Snapshot(Comparable) == Snapshot(InTyped));
	}
	InTyped = InImports.DraftHistory({InTyped.Draft, InTyped.Generation}, EImportDraftHistoryAction::Reset);
	const auto Reset = InImports.DraftHistory({InTyped.Draft, InTyped.Generation}, EImportDraftHistoryAction::Reset);
	HYP_CHECK(Snapshot(Reset) == Snapshot(InTyped));
	InImports.DiscardDraft({InTyped.Draft, InTyped.Generation, true});
	InImports.DiscardDraft({InProtocol.Draft, InProtocol.Generation, true});
}
} // namespace

void CheckImportDraftHistory(Hyperion::FTaskSystem& InTasks, Hyperion::FIOService& InIO,
                             Hyperion::FAssetImportWorkspace& InImports, const Hyperion::FImportRequest& InRequest)
{
	using namespace Hyperion;
	CheckHistoryWire();
	const auto Writes = InIO.Statistics().Writes.load();
	const auto TaskCount = InImports.List().Total;
	auto Typed = InImports.PrepareDraft(InRequest);
	const auto Preparing = Snapshot(Typed);
	CheckHistoryError(
	    [&]
	    {
		    (void)InImports.DraftHistory({Typed.Draft, Typed.Generation - 1, "unknown"});
	    },
	    "busy", "Wait for import draft work to finish");
	HYP_CHECK(Snapshot(InImports.Draft({Typed.Draft})) == Preparing);
	Typed = WaitDraft(InTasks, InImports, std::move(Typed));
	CheckRejectedHistory(InImports, Typed);
	const auto Protocol = WaitDraft(InTasks, InImports, InImports.PrepareDraft(InRequest));
	CheckHistoryParity(InImports, Typed, Protocol);
	HYP_CHECK(InIO.Statistics().Writes.load() == Writes && InImports.List().Total == TaskCount);
	HYP_CHECK(!InIO.FileSystem()->Exists(PathFromUtf8(InRequest.Output)) && InImports.DraftList().Drafts.empty());
}
