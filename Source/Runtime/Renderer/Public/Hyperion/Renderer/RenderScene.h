#pragma once
#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
struct FRenderSceneMailbox;
struct FRenderBindingResult;

enum class ERenderPrimitiveStatus
{
	PendingCreate,
	PendingResources,
	Ready,
	Removed,
	Failed
};

struct FRenderBindingStatus
{
	ERenderPrimitiveStatus State = ERenderPrimitiveStatus::PendingCreate;
	std::uint64_t Revision{};
	std::string Error;
};

struct FRenderPrimitiveUpdate
{
	FRenderPrimitiveHandle Handle;
	FRenderPrimitiveState State;
};

using FRenderPrimitiveFactory = std::function<std::unique_ptr<IRenderPrimitive>(FTaskSystem&)>;

// Main-owned move-only registration. Destruction queues removal, never deletes a proxy.
class FRenderBinding
{
public:
	FRenderBinding() = default;
	~FRenderBinding();
	FRenderBinding(FRenderBinding&& InOther) noexcept;
	FRenderBinding& operator=(FRenderBinding&& InOther) noexcept;
	FRenderBinding(const FRenderBinding&) = delete;
	FRenderBinding& operator=(const FRenderBinding&) = delete;
	FRenderPrimitiveHandle GetHandle() const;
	FRenderBindingStatus GetStatus() const;
	FTaskHandle Remove();

private:
	FRenderBinding(std::shared_ptr<FRenderSceneMailbox> InMailbox, FRenderPrimitiveHandle InHandle,
	               std::shared_ptr<FRenderBindingResult> InResult);
	std::shared_ptr<FRenderSceneMailbox> Mailbox;
	FRenderPrimitiveHandle Handle;
	std::shared_ptr<FRenderBindingResult> Result;
	FTaskHandle Removal;
	friend class FRenderSceneClient;
};

// Main facade. Dependency-free commands are serialized into the Render mailbox;
// they continue to execute without producing a GPU frame. Close before Tasks.
class FRenderSceneClient
{
public:
	explicit FRenderSceneClient(FTaskSystem& InTasks);
	~FRenderSceneClient();
	FRenderSceneClient(const FRenderSceneClient&) = delete;
	FRenderSceneClient& operator=(const FRenderSceneClient&) = delete;
	void RequireMain() const;
	FRenderBinding Create(FRenderPrimitiveState InState, FRenderPrimitiveFactory InFactory = {});
	std::vector<FRenderBinding> CreateBatch(std::vector<FRenderPrimitiveState> InStates,
	                                        FRenderPrimitiveFactory InFactory = {});
	FTaskHandle Update(std::vector<FRenderPrimitiveUpdate> InUpdates);
	FTaskHandle Flush();
	void Close();
	// Render only. The result owns all state required by subsequent frame work.
	FRenderSceneSnapshot Collect(FRenderView InView) const;

private:
	std::shared_ptr<FRenderSceneMailbox> Mailbox;
};
} // namespace Hyperion
