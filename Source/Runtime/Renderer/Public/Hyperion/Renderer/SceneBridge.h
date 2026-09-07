#pragma once
#include "Hyperion/Renderer/Model.h"

namespace Hyperion
{
class FRenderSession;

// Main-only attachment. Destroy before its session and logical scene.
class FSceneRenderBridge
{
public:
	FSceneRenderBridge(FScene& InScene, FRenderSession& InSession, FTaskSystem& InTasks);
	~FSceneRenderBridge();
	FSceneRenderBridge(const FSceneRenderBridge&) = delete;
	FSceneRenderBridge& operator=(const FSceneRenderBridge&) = delete;
	void Flush();
	void Close();
	bool IsReady(FSceneHandle InHandle) const;
	std::string GetError(FSceneHandle InHandle) const;
	std::size_t PrimitiveCount(FSceneHandle InHandle) const;

private:
	struct FAttachment
	{
		std::shared_ptr<const FSceneModelData> Data;
		std::unique_ptr<FModel> Model;
		std::string Error;
		std::uint64_t Revision{};
	};

	struct FReceipt
	{
		FSceneHandle Handle;
		FTaskHandle Task;
		std::uint64_t Revision{};
	};

	void Apply(const FSceneChange& InChange);
	void Observe(bool bInWait);
	FScene& Scene;
	FRenderSession& Session;
	FTaskSystem& Tasks;
	std::map<FSceneHandle, FAttachment> Attachments;
	std::vector<FReceipt> Receipts;
	bool bClosed{};
};
} // namespace Hyperion
