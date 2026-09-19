#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/Core/Core.h"
#include <chrono>
#include <stdexcept>

namespace Hyperion
{
void FApplicationControl::RequestExit()
{
	bExitRequested = true;
}

bool FApplicationControl::IsExitRequested() const
{
	return bExitRequested;
}

void FApplicationControl::ReportFailure(std::exception_ptr InFailure) noexcept
{
	if (!Failure)
	{
		Failure = std::move(InFailure);
	}
	bExitRequested = true;
}

void FApplicationControl::RethrowFailure() const
{
	if (Failure)
	{
		std::rethrow_exception(Failure);
	}
}

FApplicationHost::FApplicationHost(std::uint32_t InWorkers, std::uint32_t InRhiThreads)
    : Tasks(InWorkers, InRhiThreads), Services(std::make_shared<FPluginServices>())
{
	Services->AddExternal(Tasks);
	Services->AddExternal(Control);
}

FApplicationHost::~FApplicationHost()
{
	Stop();
}

void FApplicationHost::Start(const FPluginRegistry& InRegistry, const FPluginSelection& InSelection)
{
	Tasks.Require({EDomain::Main});
	if (Plugins)
	{
		throw std::logic_error("Application plugins are selected once at startup");
	}
	Plugins = std::make_unique<FPluginSet>(InRegistry.Activate(InSelection, Services));
	for (const auto& Diagnostic : Plugins->GetDiagnostics())
	{
		Log(ELogLevel::Warning, "Plugin " + Diagnostic.Id + ": " + Diagnostic.Message);
	}
}

void FApplicationHost::Run(std::uint64_t InFrames)
{
	Tasks.Require({EDomain::Main});
	if (!Plugins)
	{
		throw std::logic_error("Application host has not started");
	}
	const auto Started = std::chrono::steady_clock::now();
	auto Previous = Started;
	for (std::uint64_t Frame = 0; !Control.IsExitRequested() && (!InFrames || Frame < InFrames); ++Frame)
	{
		Tasks.PumpMain();
		const auto Now = std::chrono::steady_clock::now();
		const float Delta = Frame ? std::chrono::duration<float>(Now - Previous).count() : 1.f / 60.f;
		Previous = Now;
		Plugins->Update({Frame, std::chrono::duration<double>(Now - Started).count(), Delta});
	}
	Plugins->Quiesce();
}

void FApplicationHost::Stop() noexcept
{
	if (Plugins)
	{
		Plugins->Stop();
		if (const auto Failure = Plugins->GetStopFailure())
		{
			Control.ReportFailure(Failure);
		}
	}
}

FTaskSystem& FApplicationHost::GetTasks()
{
	return Tasks;
}

FPluginServices& FApplicationHost::GetServices()
{
	return *Services;
}

FPluginSet& FApplicationHost::GetPlugins()
{
	if (!Plugins)
	{
		throw std::logic_error("Application host has not started");
	}
	return *Plugins;
}

void TrackPluginTask(FPluginContext& InContext, FTaskSystem& InTasks, FTaskHandle InTask)
{
	try
	{
		InContext.Defer(
		    [&InTasks, InTask]
		    {
			    InTasks.Wait(InTask);
		    });
	}
	catch (...)
	{
		const auto Failure = std::current_exception();
		try
		{
			InTasks.Wait(InTask);
		}
		catch (...)
		{
		}
		std::rethrow_exception(Failure);
	}
}
} // namespace Hyperion
