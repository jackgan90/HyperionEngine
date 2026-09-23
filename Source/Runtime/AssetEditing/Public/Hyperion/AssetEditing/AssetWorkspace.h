#pragma once
#include "Hyperion/AssetEditing/AssetDocument.h"

namespace Hyperion
{
struct FAssetWorkspaceEntry
{
	std::string Id;
	std::filesystem::path Path;
	std::shared_ptr<FAssetEditDocument> Document;
	bool bEditing{};
	bool bActive{};
	std::string Error;
};

// Main-only access to the host's actual documents, including GUI-opened tabs. Borrowed snapshots
// never transfer workspace ownership. Host retirement is responsible for draining pending work.
class IAssetWorkspace
{
public:
	virtual ~IAssetWorkspace() = default;
	virtual std::string OpenDocument(const std::filesystem::path& InPath) = 0;
	virtual bool IsBlocked() const = 0;
	virtual void PumpDocument(std::string_view InId) = 0;
	virtual std::optional<FAssetWorkspaceEntry> FindDocument(std::string_view InId) const = 0;
	virtual std::vector<FAssetWorkspaceEntry> Documents() const = 0;
	virtual void ActivateDocument(std::string_view InId) = 0;
	virtual void CloseDocument(std::string_view InId) = 0;
	virtual void SetExternalEditing(std::string_view InId, bool bInEditing) = 0;
};
} // namespace Hyperion
