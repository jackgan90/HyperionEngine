#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"
#include "Hyperion/Automation/Catalog.h"
#include "Hyperion/Content/ContentRootService.h"

namespace Hyperion
{
class FImportAutomation final : public IContentRootParticipant
{
public:
	FImportAutomation(FIOService& InIO, FAssetService& InAssets, FContentRootService& InRoots);
	void Register(FOperationCatalog& InCatalog);
	void Drain();
	FContentRootParticipantState ContentRootState() const override;
	void ReleaseContentRoot() override;
	void ContentRootChanged() override;

private:
	FAssetImportService Imports;
	FAssetService& Assets;
	FContentRootService& Roots;
	std::size_t ActiveJobs{};
};
} // namespace Hyperion
