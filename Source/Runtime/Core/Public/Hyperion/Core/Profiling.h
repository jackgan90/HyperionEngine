#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

#ifndef HYP_ENABLE_PROFILING
#define HYP_ENABLE_PROFILING 0
#endif

namespace Hyperion
{
enum class EProfileCategory : std::uint32_t
{
	Frame = 1U << 0,
	Render = 1U << 1,
	Material = 1U << 2,
	Rhi = 1U << 3,
	Tasks = 1U << 4,
	Assets = 1U << 5,
	Detail = 1U << 6,
	Gpu = 1U << 7
};

constexpr std::uint32_t ProfileCategoryMask(EProfileCategory InCategory)
{
	return static_cast<std::uint32_t>(InCategory);
}

inline constexpr std::uint32_t ProfileBasicMask = 0x3f;
inline constexpr std::uint32_t ProfileAllMask = 0xff;

enum class EProfileSampling
{
	Disabled,
	Requested,
	Unavailable
};

struct FProfileStatus
{
	bool bCompiled{};
	bool bConnected{};
	std::uint32_t Mask{};
	EProfileSampling Sampling{};
};

// Control operations are serialized; scopes read only this independently published mask.
extern std::atomic<std::uint32_t> ProfileMask;
void SetProfilingMask(std::uint32_t InMask);
EProfileSampling SetProfilingSampling(bool bInEnabled);
FProfileStatus GetProfilingStatus();
std::uint64_t GetProfilingConnection();
void SetProfileThreadName(const char* InName);
void ProfileFrame();
void ProfilePlot(const char* InName, double InValue);

inline bool IsProfilingEnabled(EProfileCategory InCategory = EProfileCategory::Frame)
{
#if HYP_ENABLE_PROFILING
	return (ProfileMask.load(std::memory_order_relaxed) & ProfileCategoryMask(InCategory)) != 0;
#else
	(void)InCategory;
	return false;
#endif
}

// Macro sites and their literal strings have static lifetime, including backend storage.
struct FProfileSite
{
	const char* Name;
	const char* Function;
	const char* File;
	std::uint32_t Line;
	mutable std::atomic<const void*> Backend{};
	alignas(std::max_align_t) mutable std::byte Storage[48]{};
};

class FPerfScope
{
public:
	FPerfScope(const FProfileSite& InSite, EProfileCategory InCategory)
	{
#if HYP_ENABLE_PROFILING
		if (IsProfilingEnabled(InCategory))
		{
			Category = InCategory;
			Begin(InSite);
		}
#else
		(void)InSite;
		(void)InCategory;
#endif
	}

	~FPerfScope()
	{
#if HYP_ENABLE_PROFILING
		if (bTracked)
		{
			End();
		}
#endif
	}

	FPerfScope(const FPerfScope&) = delete;
	FPerfScope& operator=(const FPerfScope&) = delete;
	void Value(std::uint64_t InValue);

	bool IsActive() const
	{
		return bActive && IsProfilingEnabled(Category);
	}

private:
	void Begin(const FProfileSite& InSite);
	void End();
	void Open();
	void Close();
	const FProfileSite* Site{};
	FPerfScope* Previous{};
	std::uint64_t Connection{};
	std::uint64_t Annotation{};
	std::uint32_t Id{};
	EProfileCategory Category{};
	bool bActive{};
	bool bTracked{};
	bool bHasAnnotation{};
	friend class FProfileSuspension;
};

// Task adapters use this only around resumable waits; ordinary callers just use scope macros.
class FProfileSuspension
{
public:
	FProfileSuspension();
	~FProfileSuspension();
	FProfileSuspension(const FProfileSuspension&) = delete;
	FProfileSuspension& operator=(const FProfileSuspension&) = delete;

private:
	static void Resume(FPerfScope* InScope);
	FPerfScope* Chain{};
};
} // namespace Hyperion

#define HYP_PERF_JOIN_IMPL(InA, InB) InA##InB
#define HYP_PERF_JOIN(InA, InB) HYP_PERF_JOIN_IMPL(InA, InB)

#if HYP_ENABLE_PROFILING
#define HYP_PERF_SCOPE_IMPL(InCategory, InName, InVariable, InId)                                                      \
	static constinit ::Hyperion::FProfileSite HYP_PERF_JOIN(ProfileSite, InId){InName, __func__, __FILE__, __LINE__};  \
	::Hyperion::FPerfScope InVariable(HYP_PERF_JOIN(ProfileSite, InId), InCategory)
#define HYP_PERF_SCOPE_NAMED(InCategory, InName, InVariable)                                                           \
	HYP_PERF_SCOPE_IMPL(InCategory, InName, InVariable, __COUNTER__)
#define HYP_PERF_SCOPE_C(InCategory, InName)                                                                           \
	HYP_PERF_SCOPE_NAMED(::Hyperion::EProfileCategory::InCategory, #InName, HYP_PERF_JOIN(ProfileScope, __COUNTER__))
#define HYP_PERF_SCOPE(InName) HYP_PERF_SCOPE_C(Frame, InName)
#define HYP_PERF_FUNCTION()                                                                                            \
	HYP_PERF_SCOPE_NAMED(::Hyperion::EProfileCategory::Frame, __func__, HYP_PERF_JOIN(ProfileScope, __COUNTER__))
#define HYP_PERF_PLOT(InCategory, InName, InValue)                                                                     \
	do                                                                                                                 \
	{                                                                                                                  \
		if (::Hyperion::IsProfilingEnabled(::Hyperion::EProfileCategory::InCategory))                                  \
		{                                                                                                              \
			::Hyperion::ProfilePlot(#InName, double(InValue));                                                         \
		}                                                                                                              \
	} while (false)
#define HYP_PERF_VALUE(InVariable, InValue)                                                                            \
	do                                                                                                                 \
	{                                                                                                                  \
		if ((InVariable).IsActive())                                                                                   \
		{                                                                                                              \
			(InVariable).Value(InValue);                                                                               \
		}                                                                                                              \
	} while (false)
#else
#define HYP_PERF_SCOPE_NAMED(InCategory, InName, InVariable) ((void)0)
#define HYP_PERF_SCOPE_C(InCategory, InName) ((void)0)
#define HYP_PERF_SCOPE(InName) ((void)0)
#define HYP_PERF_FUNCTION() ((void)0)
#define HYP_PERF_PLOT(InCategory, InName, InValue) ((void)0)
#define HYP_PERF_VALUE(InVariable, InValue) ((void)0)
#endif
