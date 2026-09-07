#include "Hyperion/Renderer/SceneBridge.h"
#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
FSceneRenderBridge::FSceneRenderBridge(FScene& InScene, FRenderSession& InSession, FTaskSystem& InTasks)
    : Scene(InScene), Session(InSession), Tasks(InTasks)
{
	Tasks.Require({EDomain::Main});
	Scene.RequireMain();
	Scene.BeginSynchronization();
	try
	{
		Session.GetScene().AttachLogicalScene(Scene.GetIdentity());
	}
	catch (...)
	{
		Scene.EndSynchronization();
		throw;
	}
}

FSceneRenderBridge::~FSceneRenderBridge()
{
	Close();
}

void FSceneRenderBridge::Observe(bool bInWait)
{
	for (auto It = Receipts.begin(); It != Receipts.end();)
	{
		if (!bInWait && !It->Task.Ready())
		{
			++It;
			continue;
		}
		try
		{
			Tasks.Wait(It->Task);
		}
		catch (const std::exception& Error)
		{
			const auto Entry = Attachments.find(It->Handle);
			if (Entry != Attachments.end() && Entry->second.Revision == It->Revision)
			{
				Entry->second.Error = Error.what();
			}
		}
		It = Receipts.erase(It);
	}
}

void FSceneRenderBridge::Apply(const FSceneChange& InChange)
{
	auto It = Attachments.find(InChange.Handle);
	const auto Data = InChange.Model ? InChange.Model->Data : nullptr;
	if (It != Attachments.end() && (!Data || It->second.Data != Data))
	{
		It->second.Model->Remove();
		Attachments.erase(It);
		It = Attachments.end();
	}
	if (!Data)
	{
		return;
	}
	if (It == Attachments.end())
	{
		auto Model = std::make_unique<FModel>(Session.GetScene(), Session.GetResources(), *InChange.Model);
		Attachments.emplace(InChange.Handle, FAttachment{Data, std::move(Model), {}, InChange.Revision});
	}
	else
	{
		const auto Receipt = It->second.Model->SetState(*InChange.Model);
		Receipts.push_back({InChange.Handle, Receipt, InChange.Revision});
		It->second.Revision = InChange.Revision;
		It->second.Error.clear();
	}
}

void FSceneRenderBridge::Flush()
{
	Tasks.Require({EDomain::Main});
	if (bClosed)
	{
		throw std::logic_error("Scene bridge is closed");
	}
	Observe(false);
	for (const auto& Change : Scene.GetChanges())
	{
		Apply(Change);
		Scene.Acknowledge(Change.Revision);
	}
}

void FSceneRenderBridge::Close()
{
	if (bClosed)
	{
		return;
	}
	Tasks.Require({EDomain::Main});
	for (auto& [Handle, Attachment] : Attachments)
	{
		Attachment.Model->Remove();
	}
	Observe(true);
	Attachments.clear();
	Tasks.Wait(Session.GetScene().Flush());
	Session.GetScene().DetachLogicalScene(Scene.GetIdentity());
	Scene.EndSynchronization();
	bClosed = true;
}

bool FSceneRenderBridge::IsReady(FSceneHandle InHandle) const
{
	Tasks.Require({EDomain::Main});
	const auto It = Attachments.find(InHandle);
	return It != Attachments.end() && It->second.Error.empty() && It->second.Model->IsReady();
}

std::string FSceneRenderBridge::GetError(FSceneHandle InHandle) const
{
	Tasks.Require({EDomain::Main});
	const auto It = Attachments.find(InHandle);
	return It == Attachments.end()     ? std::string{}
	       : !It->second.Error.empty() ? It->second.Error
	                                   : It->second.Model->GetError();
}

std::size_t FSceneRenderBridge::PrimitiveCount(FSceneHandle InHandle) const
{
	Tasks.Require({EDomain::Main});
	const auto It = Attachments.find(InHandle);
	return It == Attachments.end() ? 0 : It->second.Model->PrimitiveCount();
}
} // namespace Hyperion
