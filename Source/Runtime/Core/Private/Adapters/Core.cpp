#include <Hyperion/Core/Core.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <mimalloc.h>
#include <new>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <tracy/TracyC.h>

namespace Hyperion
{
namespace
{
struct FCounters
{
	std::atomic_size_t Live{};
	std::atomic_size_t Peak{};
	std::atomic_size_t Count{};
};

std::array<FCounters, static_cast<std::size_t>(EMemoryTag::Count)> Counters;

struct FHeader
{
	void* Base;
	std::size_t Size;
	EMemoryTag Tag;
};

std::atomic_uint64_t ScopeCount{};
std::atomic_uint64_t ScopeTime{};
} // namespace

void InitializeLog(const std::filesystem::path& InFile)
{
	if (!InFile.parent_path().empty())
	{
		std::filesystem::create_directories(InFile.parent_path());
	}
	auto Console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto Disk = std::make_shared<spdlog::sinks::basic_file_sink_mt>(InFile.string(), false);
	auto Logger = std::make_shared<spdlog::logger>("Hyperion", spdlog::sinks_init_list{Console, Disk});
	Logger->set_pattern("[%H:%M:%S.%e] [%t] [%l] %v");
	Logger->flush_on(spdlog::level::info);
	spdlog::set_default_logger(std::move(Logger));
}

void Log(ELogLevel InLevel, std::string_view InMessage)
{
	const auto Severity = InLevel == ELogLevel::Error     ? spdlog::level::err
	                      : InLevel == ELogLevel::Warning ? spdlog::level::warn
	                                                      : spdlog::level::info;
	spdlog::log(Severity, "{}", InMessage);
}

void ShutdownLog()
{
	spdlog::shutdown();
}

void* Allocate(std::size_t InSize, std::size_t InAlignment, EMemoryTag InTag)
{
	if (!std::has_single_bit(InAlignment) || InTag >= EMemoryTag::Count)
	{
		throw std::invalid_argument("Invalid allocation alignment or tag");
	}
	InAlignment = std::max(InAlignment, alignof(FHeader));
	if (InSize > std::numeric_limits<std::size_t>::max() - InAlignment - sizeof(FHeader))
	{
		throw std::bad_alloc();
	}
	void* Base = mi_malloc(InSize + InAlignment + sizeof(FHeader));
	if (!Base)
	{
		throw std::bad_alloc();
	}
	auto Address = (reinterpret_cast<std::uintptr_t>(Base) + sizeof(FHeader) + InAlignment - 1) & ~(InAlignment - 1);
	auto* Header = reinterpret_cast<FHeader*>(Address) - 1;
	*Header = {Base, InSize, InTag};
	auto& Stats = Counters[static_cast<std::size_t>(InTag)];
	auto Live = Stats.Live.fetch_add(InSize) + InSize;
	auto Peak = Stats.Peak.load();
	while (Live > Peak && !Stats.Peak.compare_exchange_weak(Peak, Live))
	{
	}
	Stats.Count.fetch_add(1);
	void* Result = reinterpret_cast<void*>(Address);
	TracyCAlloc(Result, InSize);
	return Result;
}

void Deallocate(void* InMemory) noexcept
{
	if (!InMemory)
	{
		return;
	}
	auto* Header = static_cast<FHeader*>(InMemory) - 1;
	auto& Stats = Counters[static_cast<std::size_t>(Header->Tag)];
	Stats.Live.fetch_sub(Header->Size);
	Stats.Count.fetch_sub(1);
	TracyCFree(InMemory);
	mi_free(Header->Base);
}

FMemoryStats MemoryStats(EMemoryTag InTag)
{
	auto& Stats = Counters.at(static_cast<std::size_t>(InTag));
	return {Stats.Live.load(), Stats.Peak.load(), Stats.Count.load()};
}

void* FMemoryResource::do_allocate(std::size_t InBytes, std::size_t InAlignment)
{
	return Hyperion::Allocate(InBytes, InAlignment, Tag);
}

void FMemoryResource::do_deallocate(void* InP, std::size_t, std::size_t)
{
	Hyperion::Deallocate(InP);
}

bool FMemoryResource::do_is_equal(const std::pmr::memory_resource& InOther) const noexcept
{
	return this == &InOther;
}

std::uint64_t ClockNanoseconds()
{
	return static_cast<std::uint64_t>(
	    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
	        .count());
}

FProfileScope::FProfileScope(const char* InName)
{
	Start = ClockNanoseconds();
	TracyCZone(Context, true);
	TracyCZoneName(Context, InName, std::strlen(InName));
	Id = Context.id;
	Active = Context.active;
}

FProfileScope::~FProfileScope()
{
	ScopeTime.fetch_add(ClockNanoseconds() - Start);
	ScopeCount.fetch_add(1);
	TracyCZoneCtx Context{Id, Active};
	TracyCZoneEnd(Context);
}

FProfileStats ProfileStats()
{
	return {ScopeCount.load(), ScopeTime.load()};
}

void ProfileFrame()
{
	TracyCFrameMark;
}
} // namespace Hyperion
