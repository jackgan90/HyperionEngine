#pragma once
#include "Hyperion/Plugins/PluginRuntime.h"
#include "Hyperion/Tasks/TaskSystem.h"

namespace Hyperion
{
class FApplicationControl
{
public:
	void RequestExit();
	bool IsExitRequested() const;
	void ReportFailure(std::exception_ptr InFailure) noexcept;
	void RethrowFailure() const;

private:
	bool bExitRequested{};
	std::exception_ptr Failure;
};

class FApplicationHost
{
public:
	explicit FApplicationHost(std::uint32_t InWorkers = 4, std::uint32_t InRhiThreads = 2);
	~FApplicationHost();
	FApplicationHost(const FApplicationHost&) = delete;
	FApplicationHost& operator=(const FApplicationHost&) = delete;
	void Start(const FPluginRegistry& InRegistry, const FPluginSelection& InSelection);
	void Run(std::uint64_t InFrames = 0);
	void Stop() noexcept;
	FTaskSystem& GetTasks();
	FPluginServices& GetServices();
	FPluginSet& GetPlugins();

private:
	FTaskSystem Tasks;
	FApplicationControl Control;
	std::shared_ptr<FPluginServices> Services;
	std::unique_ptr<FPluginSet> Plugins;
};

// Track admitted work immediately. If registration fails, join before propagating that failure.
void TrackPluginTask(FPluginContext& InContext, FTaskSystem& InTasks, FTaskHandle InTask);
} // namespace Hyperion
