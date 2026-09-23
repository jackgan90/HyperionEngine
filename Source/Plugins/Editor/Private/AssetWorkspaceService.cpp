#include "AssetWorkspace.h"

namespace Hyperion
{
std::string FAssetWorkspace::OpenDocument(const std::filesystem::path& InPath)
{
	Open(InPath, {});
	return Active->DocumentId;
}

void FAssetWorkspace::SetHostBlocked(bool bInBlocked)
{
	bHostBlocked = bInBlocked;
}

bool FAssetWorkspace::IsBlocked() const
{
	return bHostBlocked || bCloseModal || bRequestClose || (Active && Active->GuiInteraction != 0);
}

std::optional<FAssetWorkspaceEntry> FAssetWorkspace::FindDocument(std::string_view InId) const
{
	for (const auto& Entry : Entries)
	{
		if (Entry->DocumentId == InId)
		{
			return FAssetWorkspaceEntry{
			    Entry->DocumentId,     Entry->Path,
			    Entry->Document,       IsBlocked() || Entry->GuiInteraction != 0 || Entry->HasPendingEdit(),
			    Entry.get() == Active, Entry->Document ? std::string{} : Entry->Error};
		}
	}
	return {};
}

std::vector<FAssetWorkspaceEntry> FAssetWorkspace::Documents() const
{
	std::vector<FAssetWorkspaceEntry> Result;
	for (const auto& Entry : Entries)
	{
		Result.push_back(*FindDocument(Entry->DocumentId));
	}
	return Result;
}

void FAssetWorkspace::ActivateDocument(std::string_view InId)
{
	for (const auto& Entry : Entries)
	{
		if (Entry->DocumentId == InId)
		{
			Active = Entry.get();
			Active->bActivate = true;
			return;
		}
	}
	throw std::invalid_argument("Unknown workspace document");
}

void FAssetWorkspace::CloseDocument(std::string_view InId)
{
	for (const auto& Entry : Entries)
	{
		if (Entry->DocumentId == InId)
		{
			Close(*Entry);
			return;
		}
	}
	throw std::invalid_argument("Unknown workspace document");
}

void FAssetWorkspace::SetExternalEditing(std::string_view InId, bool bInEditing)
{
	for (const auto& Entry : Entries)
	{
		if (Entry->DocumentId == InId)
		{
			Entry->bExternalEditing = bInEditing;
			return;
		}
	}
}

void FAssetWorkspace::PumpDocument(std::string_view InId)
{
	for (const auto& Entry : Entries)
	{
		if (Entry->DocumentId == InId)
		{
			PollEntry(*Entry);
			if (Entry->Document)
			{
				if (auto Saved = Entry->Document->PollSave())
				{
					ExternalSaved.push_back(std::move(*Saved));
				}
			}
			return;
		}
	}
}
} // namespace Hyperion
