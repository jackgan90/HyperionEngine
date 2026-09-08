#pragma once
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Support/TestSupport.h"

namespace Hyperion::InstanceTests
{
FMaterialValue Payload(float InRed = .2f);
FMaterialDescription Description();
std::shared_ptr<const FMaterialSnapshot> Surface(FMaterialDescription InDescription = Description());
FRenderResourceDesc Geometry(std::shared_ptr<const FMaterialSnapshot> InSurface);
void WaitFor(const std::function<bool()>& InCondition);

struct FEmission
{
	std::size_t Count = 8;
	std::optional<std::size_t> Hidden;
	std::optional<std::size_t> Changed;
	std::optional<std::size_t> Invalid;
	std::optional<std::size_t> Dynamic;
	std::optional<std::size_t> Alternate;
	std::shared_ptr<const FRenderMaterial> Other;
	bool bReverse{};
	bool bStable = true;
};

class FItems final : public IRenderPrimitive
{
public:
	FItems(FTaskSystem& InTasks, std::shared_ptr<FEmission> InEmission);
	void Collect(const FRenderView& InView, std::vector<FRenderItem>& OutItems) const override;

private:
	std::shared_ptr<FEmission> Emission;
};

struct FFixture
{
	FTaskSystem Tasks{2, 1};
	FWindow Window{"Instance batch tests", {128, 96}, true};
	FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "Source/Tests/Renderer", "instance-test/cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	std::shared_ptr<const FRenderResource> Resource;
	std::shared_ptr<FEmission> Emission = std::make_shared<FEmission>();
	FRenderBinding Binding;
	FRenderView View;
	FSceneVisibilityStats Statistics;
	FFixture();
	~FFixture();
	std::vector<FPassCommands> Build(bool bInBatch = true, std::span<const FRenderView> InViews = {});
	FImage Draw(const std::vector<FPassCommands>& InPasses);
	std::shared_ptr<const FRenderMaterial> Material(FMaterialDescription InDescription);
};

std::size_t DrawCount(const std::vector<FPassCommands>& InPasses);
std::size_t InstanceCount(const std::vector<FPassCommands>& InPasses);
void RunInstanceCacheTests(FFixture& InFixture);
void RunInstanceFailureTests(FFixture& InFixture);
void RunInstanceContractTests(FShaderCompiler& InCompiler);
FRenderSceneSnapshot Snapshot(FFixture& InFixture, std::size_t InCount);
std::vector<FPassCommands> Prepare(FFixture& InFixture, FRenderBatchSystem& InBatches, FRenderSceneSnapshot InSnapshot);
void RunInstancePlanningTests(FFixture& InFixture);
void RunInstanceResourceTests(FFixture& InFixture);
} // namespace Hyperion::InstanceTests
