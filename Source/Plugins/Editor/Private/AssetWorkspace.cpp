#include "AssetWorkspace.h"
#include "Hyperion/Core/Identity.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>

namespace Hyperion
{
FAssetWorkspace::FAssetWorkspace(FAssetService& InAssets, FTaskSystem& InTasks, FRenderSession& InSession,
                                 FRHICapabilities InCapabilities)
    : Assets(InAssets), Tasks(InTasks), Session(InSession), Capabilities(InCapabilities)
{
	WorkspaceIdentity = CreateEphemeralIdentity();
}

FAssetWorkspace::~FAssetWorkspace()
{
	CloseAll();
}

void FAssetWorkspace::Open(const std::filesystem::path& InPath, std::string_view InIdentity)
{
	const auto Path = Assets.NormalizePath(InPath);
	for (const auto& Entry : Entries)
	{
		if (Entry->Path == Path || (!InIdentity.empty() && Entry->Identity == InIdentity))
		{
			Active = Entry.get();
			Active->bActivate = true;
			return;
		}
	}
	RefreshIndex();
	auto Entry = std::make_unique<FEntry>();
	Entry->DocumentId = WorkspaceIdentity + "/document/" + std::to_string(++NextDocument);
	Entry->Path = Path;
	Entry->Identity = InIdentity;
	Entry->TextureId = NextTexture++;
	Entry->bReadOnly = IsAssetPathReadOnly(Assets, Path);
	Assets.Invalidate(Path);
	Entry->Load = Assets.LoadAsync(Path);
	Active = Entry.get();
	Entries.push_back(std::move(Entry));
}

void FAssetWorkspace::Close(FEntry& InEntry)
{
	Bounds.erase("tab/" + PathToUtf8(InEntry.Path));
	InEntry.Cancellation.Cancel();
	InEntry.Load.Cancel();
	for (const auto Task : {InEntry.Initialization ? InEntry.Initialization->Task() : FTaskHandle{},
	                        InEntry.EncodingEdit ? InEntry.EncodingEdit->Task() : FTaskHandle{},
	                        InEntry.Pending ? InEntry.Pending->Task() : FTaskHandle{},
	                        InEntry.Texture.Upload ? InEntry.Texture.Upload->Task() : FTaskHandle{},
	                        InEntry.Texture.Pending ? InEntry.Texture.Pending->Task() : FTaskHandle{}})
	{
		try
		{
			Tasks.Wait(Task);
		}
		catch (...)
		{
		}
	}
	InEntry.Scene.reset();
	InEntry.Pipeline.reset();
	InEntry.PreviewSession.reset();
	if (Active == &InEntry)
	{
		Active = nullptr;
	}
	if (Closing == &InEntry)
	{
		Closing = nullptr;
	}
	std::erase_if(Entries,
	              [&](const auto& InItem)
	              {
		              return InItem.get() == &InEntry;
	              });
	if (!Active && !Entries.empty())
	{
		Active = Entries.back().get();
		Active->bActivate = true;
	}
}

void FAssetWorkspace::CloseAll()
{
	while (!Entries.empty())
	{
		Close(*Entries.back());
	}
	AssetIndex.clear();
	ReferenceChoices.clear();
	bRequestClose = bCloseModal = false;
	Bounds.clear();
}

bool FAssetWorkspace::HasDocuments() const
{
	return !Entries.empty();
}

bool FAssetWorkspace::IsClosePending() const
{
	return Closing != nullptr;
}

void FAssetWorkspace::CancelCloseDialog()
{
	Closing = nullptr;
	bCloseModal = bRequestClose = false;
}

void FAssetWorkspace::SuspendInput(std::span<const FInputEvent> InEvents)
{
	for (const auto& Entry : Entries)
	{
		Entry->Navigation.SuspendInput(InEvents);
		Entry->Texture.bDragging = false;
	}
}

bool FAssetWorkspace::HasActive() const
{
	return Active != nullptr;
}

const FAssetEditDocument* FAssetWorkspace::ActiveDocument() const
{
	return Active ? Active->Document.get() : nullptr;
}

bool FAssetWorkspace::IsPreviewReady() const
{
	return Active && Active->Document && Active->Preview && !Active->Pending && Active->Error.empty() &&
	       Active->PreparedGeneration == Active->Document->PreviewGeneration() &&
	       (Active->Scene
	            ? Active->Scene->GetStatus().bReady
	            : Active->Texture.Target.Texture && Active->Texture.TargetRevision == Active->Texture.Revision);
}

std::string FAssetWorkspace::ActiveStatus() const
{
	if (!Active)
	{
		return "No asset editor selected";
	}
	if (!Active->Error.empty())
	{
		return PathToUtf8(Active->Path) + ": " + Active->Error;
	}
	if (Active->Document && !Active->Document->Error.empty())
	{
		return Active->Document->Error;
	}
	if (Active->Scene)
	{
		const auto& Status = Active->Scene->GetStatus();
		if (!Status.Error.empty())
		{
			return Status.Error;
		}
		if (!Status.PublicationError.empty())
		{
			return Status.PublicationError;
		}
		for (const auto Handle : Active->Scene->GetNodes(ESceneNodeKind::EnvironmentLight))
		{
			const auto SkyStatus = Active->Scene->GetSkyStatus(Handle);
			if (SkyStatus.starts_with("Failed:"))
			{
				return SkyStatus;
			}
		}
		for (const auto Handle : Active->Scene->GetHandles())
		{
			const auto Error = Active->Scene->GetError(Handle);
			if (!Error.empty())
			{
				return Error;
			}
		}
		return PathToUtf8(Active->Path) + ": " + std::to_string(Status.ReadyModels) + "/" +
		       std::to_string(Status.Models) + " models ready";
	}
	return PathToUtf8(Active->Path) + (IsPreviewReady() ? ": Ready" : ": Preparing");
}

FVec4 FAssetWorkspace::ObservedBounds(std::string_view InId) const
{
	const auto It = Bounds.find(InId);
	return It == Bounds.end() ? FVec4{} : It->second;
}

bool FAssetWorkspace::HasActiveInteraction() const
{
	return Active && Active->GuiInteraction != 0;
}

bool FAssetWorkspace::CanUndo() const
{
	return Active && Active->Document && !Active->HasPendingEdit() && Active->Document->CanUndo();
}

bool FAssetWorkspace::CanRedo() const
{
	return Active && Active->Document && !Active->HasPendingEdit() && Active->Document->CanRedo();
}

void FAssetWorkspace::Undo()
{
	if (CanUndo())
	{
		Active->Document->Undo();
	}
}

void FAssetWorkspace::Redo()
{
	if (CanRedo())
	{
		Active->Document->Redo();
	}
}

bool FAssetWorkspace::IsDirty() const
{
	return std::any_of(Entries.begin(), Entries.end(),
	                   [](const auto& InItem)
	                   {
		                   return InItem->HasPendingEdit() || (InItem->Document && InItem->Document->IsDirty());
	                   });
}

bool FAssetWorkspace::HasPendingEdits() const
{
	return std::any_of(Entries.begin(), Entries.end(),
	                   [](const auto& InItem)
	                   {
		                   return InItem->HasPendingEdit();
	                   });
}

bool FAssetWorkspace::IsSaving() const
{
	return std::any_of(Entries.begin(), Entries.end(),
	                   [](const auto& InItem)
	                   {
		                   return InItem->bSaveRequested || (InItem->Document && InItem->Document->IsSaving());
	                   });
}

void FAssetWorkspace::SaveActive()
{
	if (Active && Active->Document && !Active->bReadOnly)
	{
		try
		{
			if (Active->HasPendingEdit() || Active->Document->IsSaving())
			{
				Active->bSaveRequested = true;
			}
			else
			{
				Active->Document->Save(Assets);
			}
		}
		catch (const std::exception& Failure)
		{
			Active->Document->Error = Failure.what();
		}
	}
}

void FAssetWorkspace::SaveAll()
{
	for (const auto& Entry : Entries)
	{
		if (Entry->Document && (Entry->Document->IsDirty() || Entry->HasPendingEdit()) && !Entry->bReadOnly)
		{
			try
			{
				if (Entry->HasPendingEdit() || Entry->Document->IsSaving())
				{
					Entry->bSaveRequested = true;
				}
				else
				{
					Entry->Document->Save(Assets);
				}
			}
			catch (const std::exception& Failure)
			{
				Entry->Document->Error = Failure.what();
			}
		}
	}
}

std::vector<FAssetSaveResult> FAssetWorkspace::Poll()
{
	// Close only between frames: the previous GUI submission may still reference this preview.
	for (std::size_t IndexInEntries = Entries.size(); IndexInEntries > 0; --IndexInEntries)
	{
		auto& Entry = *Entries[IndexInEntries - 1];
		if (Entry.bClosePending)
		{
			Close(Entry);
		}
	}
	auto Saved = std::exchange(ExternalSaved, {});
	for (const auto& Entry : Entries)
	{
		PollEntry(*Entry);
		if (Entry->Document)
		{
			if (auto Result = Entry->Document->PollSave())
			{
				Saved.push_back(std::move(*Result));
			}
		}
	}
	if (!Saved.empty())
	{
		RefreshDependencies(Saved);
	}
	for (std::size_t IndexInEntries = Entries.size(); IndexInEntries > 0; --IndexInEntries)
	{
		auto& Entry = *Entries[IndexInEntries - 1];
		if (Entry.bCloseAfterSave && !Entry.HasPendingEdit() && !Entry.Document->IsSaving())
		{
			Entry.bCloseAfterSave = false;
			if (!Entry.Document->IsDirty())
			{
				Close(Entry);
			}
		}
	}
	return Saved;
}

void FAssetWorkspace::BeginFrame()
{
	for (const auto& Entry : Entries)
	{
		Entry->bVisible = false;
	}
}

void FAssetWorkspace::RefreshIndex()
{
	AssetIndex = Assets.GetAssetIndex();
	ReferenceChoices.clear();
}

void FAssetWorkspace::RefreshDependencies(std::span<const FAssetSaveResult> InSaved)
{
	RefreshIndex();
	for (const auto& Entry : Entries)
	{
		bool bAffected = Entry->Pending.has_value() || (!Entry->Preview && !Entry->Error.empty());
		for (const auto& Saved : InSaved)
		{
			bAffected |= Entry->Dependencies.contains(Saved.Header.Id);
		}
		if (!bAffected)
		{
			continue;
		}
		// A pending generation must finish before a replacement is admitted.
		Entry->PreparedGeneration = 0;
		Entry->RequestedGeneration = 0;
	}
}

void FAssetWorkspace::PollEntry(FEntry& InEntry)
{
	try
	{
		if (InEntry.Initialization && InEntry.Initialization->Ready())
		{
			InEntry.Document = *InEntry.Initialization->GetReady();
			InEntry.Initialization.reset();
		}
		if (!InEntry.Document && !InEntry.Initialization && InEntry.Error.empty() && InEntry.Load.Ready())
		{
			const auto Loaded = InEntry.Load.GetReady();
			InEntry.Identity = Loaded->Header.Id;
			const auto& Type = Loaded->Header.TypeId;
			if (Type != "hyperion.textureasset" && Type != "hyperion.modelasset" && Type != "hyperion.materialasset" &&
			    Type != "hyperion.skyasset")
			{
				throw std::runtime_error("Unsupported asset type: " + Type);
			}
			InEntry.Initialization = DispatchAsync<std::shared_ptr<FAssetEditDocument>>(
			    Tasks, {EDomain::Worker},
			    [Loaded]
			    {
				    return std::make_shared<FAssetEditDocument>(Loaded);
			    },
			    InEntry.Cancellation);
		}
		PollReferenceEdit(InEntry);
		if (InEntry.EncodingEdit && InEntry.EncodingEdit->Ready())
		{
			const auto Texture = InEntry.EncodingEdit->GetReady();
			InEntry.EncodingEdit.reset();
			if (InEntry.EncodingGeneration == InEntry.Document->Generation())
			{
				InEntry.Document->Set({}, *Texture);
			}
		}
		if (InEntry.bSaveRequested && !InEntry.HasPendingEdit() && !InEntry.Document->IsSaving())
		{
			InEntry.bSaveRequested = false;
			InEntry.Document->Save(Assets);
		}
		if (InEntry.Pending && InEntry.Pending->Ready())
		{
			const auto Generation = InEntry.RequestedGeneration;
			auto Result = InEntry.Pending->GetReady();
			InEntry.Pending.reset();
			if (Generation == InEntry.Document->PreviewGeneration())
			{
				InEntry.Dependencies = Result->Dependencies;
				if (Result->Error.empty())
				{
					Publish(InEntry, *Result);
					InEntry.PreparedGeneration = Generation;
				}
				else
				{
					InEntry.Error = Result->Error;
				}
			}
		}
		if (InEntry.Document && !InEntry.Pending && &InEntry == Active &&
		    InEntry.RequestedGeneration != InEntry.Document->PreviewGeneration())
		{
			InEntry.RequestedGeneration = InEntry.Document->PreviewGeneration();
			const auto Loaded = InEntry.Document->Loaded();
			InEntry.Pending = DispatchAsync<FPrepared>(
			    Tasks, {EDomain::Worker},
			    [this, Loaded, Draft = InEntry.Document->Snapshot(), Shape = InEntry.Shape,
			     Token = InEntry.Cancellation, Existing = InEntry.PreviewModel]() mutable
			    {
				    return Prepare(Loaded, std::move(Draft), Shape, Token, Existing);
			    },
			    InEntry.Cancellation);
		}
		if (InEntry.Scene)
		{
			InEntry.Scene->Tick();
		}
	}
	catch (const std::exception& Failure)
	{
		if (InEntry.Pending && InEntry.Pending->Ready())
		{
			InEntry.Pending.reset();
		}
		if (InEntry.Initialization && InEntry.Initialization->Ready())
		{
			InEntry.Initialization.reset();
		}
		if (InEntry.EncodingEdit && InEntry.EncodingEdit->Ready())
		{
			InEntry.EncodingEdit.reset();
			InEntry.bSaveRequested = false;
		}
		InEntry.Error = Failure.what();
	}
}

void FAssetWorkspace::DrawTabs(FGui& InGui, float InDelta, std::span<const FInputEvent> InEvents)
{
	for (const auto& Entry : Entries)
	{
		Entry->bVisible = false;
	}
	if (Entries.empty())
	{
		InGui.TextWrapped("Double-click an asset in the main window's Content Browser to open it here.");
		return;
	}
	if (!InGui.BeginTabBar("AssetDocuments"))
	{
		return;
	}
	for (const auto& Entry : Entries)
	{
		bool bOpen = true;
		const auto Label = PathToUtf8(Entry->Path.filename()) +
		                   (Entry->HasPendingEdit() || (Entry->Document && Entry->Document->IsDirty()) ? " *" : "") +
		                   "###asset-" + std::to_string(Entry->TextureId);
		const bool bSelected = InGui.BeginTabItem(Label.c_str(), &bOpen, std::exchange(Entry->bActivate, false));
		Bounds["tab/" + PathToUtf8(Entry->Path)] = InGui.LastItemBounds();
		if (bSelected)
		{
			Active = Entry.get();
			Entry->bVisible = true;
			DrawPreview(InGui, *Entry, InDelta, InEvents);
			InGui.EndTabItem();
		}
		else
		{
			Entry->Navigation.SuspendInput(InEvents);
		}
		if (!bOpen)
		{
			if (Entry->Document &&
			    (Entry->HasPendingEdit() || Entry->Document->IsDirty() || Entry->Document->IsSaving()))
			{
				Closing = Entry.get();
				bRequestClose = true;
			}
			else
			{
				Entry->bClosePending = true;
			}
		}
	}
	InGui.EndTabBar();
}

void FAssetWorkspace::DrawCloseDialog(FGui& InGui)
{
	Bounds.erase("close/save");
	Bounds.erase("close/cancel");
	Bounds.erase("close/discard");
	if (std::exchange(bRequestClose, false))
	{
		InGui.OpenPopup("Unsaved Asset");
		bCloseModal = true;
	}
	if (Closing && InGui.BeginModal("Unsaved Asset", bCloseModal))
	{
		InGui.TextWrapped("Save changes to " + PathToUtf8(Closing->Path.filename()) + "?");
		if (Closing->HasPendingEdit())
		{
			InGui.TextWrapped("Wait for the pending edit to finish before saving, or discard it explicitly.");
		}
		if (InGui.Button("Save and Close", !Closing->HasPendingEdit() && !Closing->Document->IsSaving()))
		{
			try
			{
				Closing->Document->Save(Assets);
				Closing->bCloseAfterSave = true;
				InGui.ClosePopup();
				bCloseModal = false;
			}
			catch (const std::exception& Failure)
			{
				Closing->Document->Error = Failure.what();
			}
		}
		Bounds["close/save"] = InGui.LastItemBounds();
		InGui.SameLine();
		if (InGui.Button("Discard", !Closing->Document->IsSaving()))
		{
			Closing->bClosePending = true;
			Closing = nullptr;
			InGui.ClosePopup();
		}
		Bounds["close/discard"] = InGui.LastItemBounds();
		InGui.SameLine();
		if (InGui.Button("Cancel"))
		{
			Closing = nullptr;
			InGui.ClosePopup();
		}
		Bounds["close/cancel"] = InGui.LastItemBounds();
		InGui.EndModal();
	}
	if (!bCloseModal)
	{
		Closing = nullptr;
	}
}
} // namespace Hyperion
