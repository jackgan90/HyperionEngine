#include "ProfilingInternal.h"
#include <mutex>
#include <new>
#if HYP_ENABLE_PROFILING
#include <tracy/Tracy.hpp>
#endif

namespace Hyperion
{
namespace
{
thread_local FPerfScope* CurrentScope{};
}

#if HYP_ENABLE_PROFILING
const ___tracy_source_location_data* ProfileSource(const FProfileSite& InSite)
{
	const void* Native = InSite.Backend.load(std::memory_order_acquire);
	if (!Native)
	{
		static std::mutex Registration;
		std::lock_guard Lock(Registration);
		Native = InSite.Backend.load(std::memory_order_relaxed);
		if (!Native)
		{
			static_assert(sizeof(___tracy_source_location_data) <= sizeof(InSite.Storage));
			static_assert(alignof(___tracy_source_location_data) <= alignof(FProfileSite));
			Native = new (InSite.Storage)
			    ___tracy_source_location_data{InSite.Name, InSite.Function, InSite.File, InSite.Line, 0};
			InSite.Backend.store(Native, std::memory_order_release);
		}
	}
	return static_cast<const ___tracy_source_location_data*>(Native);
}
#endif

void FPerfScope::Open()
{
#if HYP_ENABLE_PROFILING
	bActive = false;
	if (!TracyCIsConnected)
	{
		return;
	}
	const auto Context = ___tracy_emit_zone_begin(ProfileSource(*Site), true);
	bActive = Context.active != 0;
	if (bActive)
	{
		Id = Context.id;
		Connection = Context.connectionId;
		if (bHasAnnotation)
		{
			Value(Annotation);
		}
	}
#endif
}

void FPerfScope::Close()
{
#if HYP_ENABLE_PROFILING
	if (bActive)
	{
		TracyCZoneCtx Context{Id, 1, Connection};
		TracyCZoneEnd(Context);
		bActive = false;
	}
#endif
}

void FPerfScope::Begin(const FProfileSite& InSite)
{
#if HYP_ENABLE_PROFILING
	Site = &InSite;
	Open();
	if (bActive)
	{
		Previous = CurrentScope;
		CurrentScope = this;
		bTracked = true;
	}
#else
	(void)InSite;
#endif
}

void FPerfScope::End()
{
	Close();
	CurrentScope = Previous;
	bTracked = false;
}

void FPerfScope::Value(std::uint64_t InValue)
{
#if HYP_ENABLE_PROFILING
	Annotation = InValue;
	bHasAnnotation = true;
	if (bActive)
	{
		TracyCZoneCtx Context{Id, 1, Connection};
		TracyCZoneValue(Context, InValue);
	}
#else
	(void)InValue;
#endif
}

FProfileSuspension::FProfileSuspension()
{
	Chain = CurrentScope;
	for (auto* Scope = Chain; Scope; Scope = Scope->Previous)
	{
		Scope->Close();
	}
	CurrentScope = nullptr;
}

void FProfileSuspension::Resume(FPerfScope* InScope)
{
	if (InScope)
	{
		Resume(InScope->Previous);
		// Reopen execution segments only when collection remains enabled.
		if (IsProfilingEnabled(InScope->Category))
		{
			InScope->Open();
		}
	}
}

FProfileSuspension::~FProfileSuspension()
{
	Resume(Chain);
	CurrentScope = Chain;
}

void ProfileFrame()
{
#if HYP_ENABLE_PROFILING
	if (ProfileMask.load(std::memory_order_relaxed))
	{
		TracyCFrameMark;
	}
#endif
}

void ProfilePlot(const char* InName, double InValue)
{
#if HYP_ENABLE_PROFILING
	TracyCPlot(InName, InValue);
#else
	(void)InName;
	(void)InValue;
#endif
}

void SetProfileThreadName(const char* InName)
{
#if HYP_ENABLE_PROFILING
	tracy::SetThreadName(InName);
#else
	(void)InName;
#endif
}

std::uint64_t GetProfilingConnection()
{
#if HYP_ENABLE_PROFILING
	return TracyCIsConnected ? tracy::GetProfiler().ConnectionId() : 0;
#else
	return 0;
#endif
}
} // namespace Hyperion
