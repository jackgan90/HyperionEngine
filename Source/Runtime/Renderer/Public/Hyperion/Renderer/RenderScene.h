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
	FRenderDrawResult GetLastDrawResult() const;
	FTaskHandle Remove();
	static FTaskHandle RemoveBatch(std::span<FRenderBinding> InBindings);

private:
	FRenderBinding(std::shared_ptr<FRenderSceneMailbox> InMailbox, FRenderPrimitiveHandle InHandle,
	               std::shared_ptr<FRenderBindingResult> InResult);
	std::shared_ptr<FRenderSceneMailbox> Mailbox;
	FRenderPrimitiveHandle Handle;
	std::shared_ptr<FRenderBindingResult> Result;
	FTaskHandle Removal;
	friend class FRenderSceneClient;
};

struct FRenderScenePublication
{
	std::vector<std::vector<FRenderBinding>> Groups;
	FTaskHandle Task;
};

// Main facade. Dependency-free commands are serialized into the Render mailbox;
// they continue to execute without producing a GPU frame. Close before Tasks.
class FRenderSceneClient
{
public:
	explicit FRenderSceneClient(FTaskSystem& InTasks, std::function<std::shared_ptr<const void>()> InScopeFactory = {},
	                            std::function<void()> InOnChanged = {});
	~FRenderSceneClient();
	FRenderSceneClient(const FRenderSceneClient&) = delete;
	FRenderSceneClient& operator=(const FRenderSceneClient&) = delete;
	void RequireMain() const;
	std::uint64_t GetLogicalSceneIdentity() const;
	void AttachLogicalScene(std::uint64_t InIdentity);
	void DetachLogicalScene(std::uint64_t InIdentity);
	FRenderBinding Create(FRenderPrimitiveState InState, FRenderPrimitiveFactory InFactory = {});
	std::vector<FRenderBinding> CreateBatch(std::vector<FRenderPrimitiveState> InStates,
	                                        FRenderPrimitiveFactory InFactory = {});
	FTaskHandle Update(std::vector<FRenderPrimitiveUpdate> InUpdates);
	// One admission for a material bridge's new groups and already validated existing-object updates.
	FRenderScenePublication PublishGroups(std::vector<std::vector<FRenderPrimitiveState>> InGroups,
	                                      std::vector<FRenderPrimitiveUpdate> InUpdates,
	                                      std::vector<FRenderPrimitiveHandle> InRemovals = {});
	FTaskHandle RemoveBatch(std::span<FRenderBinding> InBindings);
	FTaskHandle Flush();
	void Close();
	// Render only. The result owns all state required by subsequent frame work.
	FRenderSceneSnapshot Collect(FRenderView InView, bool bInRefresh = true, std::uint64_t InResourceRevision = 0,
	                             FRenderSceneSnapshot* InPrevious = nullptr) const;
	FSceneVisibilityStats BeginViews() const;
	std::optional<std::uint64_t> GetCollectionRevision() const;
	std::vector<FBounds> QueryBounds(const ISceneVisibility& InVisibility) const;

private:
	std::shared_ptr<FRenderSceneMailbox> Mailbox;
};
} // namespace Hyperion
