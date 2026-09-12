#include "RenderSceneInternal.h"
#include <stdexcept>

namespace Hyperion
{
void FRenderSceneClient::ValidateAdmittedToken(FScenePublicationToken InToken) const
{
	RequireMain();
	std::lock_guard Lock(Mailbox->Admission);
	if (!InToken.PublicationSerial || InToken != Mailbox->AdmittedToken ||
	    InToken.AttachmentEpoch != Mailbox->AttachmentEpoch || InToken.LogicalSceneIdentity != Mailbox->LogicalScene)
	{
		throw std::invalid_argument("Foreign, stale or uninitialized scene publication token");
	}
}

std::shared_ptr<const FSceneMetadata> FRenderSceneClient::ResolveMetadata(FScenePublicationToken InToken) const
{
	Mailbox->Tasks.Require({EDomain::Render});
	return Mailbox->Scene->ResolveMetadata(InToken);
}

void FRenderScene::RequireHealthy() const
{
	Tasks.Require({EDomain::Render});
	if (bPublicationFailed)
	{
		throw std::runtime_error("Scene publication failed; close and reattach before building scene frames");
	}
}

void FRenderScene::BeginPublication(const FSceneMetadata& InMetadata, bool bInCleanup)
{
	Tasks.Require({EDomain::Render});
	const bool bNewAttachment = !Metadata || Metadata->Token.AttachmentEpoch != InMetadata.Token.AttachmentEpoch;
	if (bNewAttachment)
	{
		if (!bInCleanup && (InMetadata.Token.PublicationSerial != 1 || !Entries.empty()))
		{
			throw std::logic_error("Scene attachment must begin empty with publication serial one");
		}
		bPublicationFailed = false;
	}
	if (!bInCleanup)
	{
		RequireHealthy();
	}
}

void FRenderScene::CompletePublication(std::shared_ptr<const FSceneMetadata> InMetadata)
{
	Tasks.Require({EDomain::Render});
	Metadata = std::move(InMetadata);
}

void FRenderScene::FailPublication() noexcept
{
	bPublicationFailed = true;
}

std::shared_ptr<const FSceneMetadata> FRenderScene::ResolveMetadata(FScenePublicationToken InToken) const
{
	RequireHealthy();
	if (!Metadata || !InToken.PublicationSerial || Metadata->Token != InToken)
	{
		throw std::invalid_argument("Scene frame token does not exactly match the applied publication");
	}
	return Metadata;
}
} // namespace Hyperion
