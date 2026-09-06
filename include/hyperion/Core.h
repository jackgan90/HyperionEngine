#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory_resource>
#include <string_view>

namespace Hyperion
{
enum class ELogLevel
{
	Info,
	Warning,
	Error
};
void InitializeLog(const std::filesystem::path& InFile);
void Log(ELogLevel InLevel, std::string_view InMessage);
void ShutdownLog();
enum class EMemoryTag : std::uint8_t
{
	Core,
	Tasks,
	Assets,
	Render,
	Gui,
	Count
};

struct FMemoryStats
{
	std::size_t LiveBytes{};
	std::size_t PeakBytes{};
	std::size_t Allocations{};
};

void* Allocate(std::size_t InSize, std::size_t InAlignment = alignof(std::max_align_t),
               EMemoryTag InTag = EMemoryTag::Core);
void Deallocate(void* InMemory) noexcept;
FMemoryStats MemoryStats(EMemoryTag InTag);

class FMemoryResource final : public std::pmr::memory_resource
{
public:
	explicit FMemoryResource(EMemoryTag InTag) : Tag(InTag)
	{
	}

private:
	void* do_allocate(std::size_t InBytes, std::size_t InAlignment) override;
	void do_deallocate(void* InP, std::size_t, std::size_t) override;
	bool do_is_equal(const std::pmr::memory_resource& InOther) const noexcept override;
	EMemoryTag Tag;
};

std::uint64_t ClockNanoseconds();

struct FProfileStats
{
	std::uint64_t Scopes{};
	std::uint64_t Nanoseconds{};
};

FProfileStats ProfileStats();

class FProfileScope
{
public:
	explicit FProfileScope(const char* InName);
	~FProfileScope();
	FProfileScope(const FProfileScope&) = delete;
	FProfileScope& operator=(const FProfileScope&) = delete;

private:
	std::uint32_t Id{};
	int Active{};
	std::uint64_t Start{};
};

void ProfileFrame();
} // namespace Hyperion
