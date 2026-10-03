#pragma once
#include <memory>

namespace Hyperion
{
class FWindow;
struct FWindowGroupState;
struct FWindowMembership;

// Owner-thread only. Release before destroying the registered window.
class FWindowRegistration
{
public:
	FWindowRegistration() = default;
	~FWindowRegistration();
	FWindowRegistration(FWindowRegistration&& InOther) noexcept;
	FWindowRegistration& operator=(FWindowRegistration&& InOther) noexcept;
	FWindowRegistration(const FWindowRegistration&) = delete;
	FWindowRegistration& operator=(const FWindowRegistration&) = delete;
	void Reset() noexcept;
	bool IsInputBlocked() const;

private:
	friend class FWindowGroup;
	explicit FWindowRegistration(std::shared_ptr<FWindowMembership> InMembership);
	std::shared_ptr<FWindowMembership> Membership;
};

// Main and members must share the calling thread. Native owner/enabled state is
// exclusively coordinated by the group until registration ends. No nested groups.
class FWindowGroup
{
public:
	explicit FWindowGroup(FWindow& InMain);
	~FWindowGroup();
	FWindowGroup(const FWindowGroup&) = delete;
	FWindowGroup& operator=(const FWindowGroup&) = delete;
	FWindowRegistration Register(FWindow& InWindow);
	void SetModalActive(bool bInActive);
	// Call before input admission and after modal changes, even when Main is minimized.
	void Synchronize();
	bool IsInputBlocked(const FWindow& InWindow) const;

private:
	std::shared_ptr<FWindowGroupState> State;
};
} // namespace Hyperion
