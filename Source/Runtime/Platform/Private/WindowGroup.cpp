#include "Adapters/WindowModality.h"
#include "WindowGroupHooks.h"
#include <Hyperion/Core/Core.h>
#include <Hyperion/Platform/WindowGroup.h>
#include <map>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace Hyperion
{
struct FWindowMembership
{
	std::weak_ptr<FWindowGroupState> Group;
	FWindow* Window{};
	FNativeWindowModality Native;

	FWindowMembership(FWindow& InMain, FWindow& InWindow)
	    : Window(&InWindow), Native(InMain.Surface(), InWindow.Surface())
	{
	}
};

struct FWindowGroupState
{
	FWindow* Main{};
	std::thread::id Thread = std::this_thread::get_id();
	std::map<const FWindow*, std::shared_ptr<FWindowMembership>> Members;
	bool bModal{};

	void RequireThread() const
	{
		if (Thread != std::this_thread::get_id())
		{
			throw std::logic_error("Window group operation requires the window owner thread");
		}
	}
};

namespace
{
thread_local std::map<const FWindow*, std::weak_ptr<FWindowGroupState>> Groups;

void ReportCleanupFailure() noexcept
{
	try
	{
		Log(ELogLevel::Error, "Window group cleanup failed to restore native state");
	}
	catch (...)
	{
	}
}

void RemoveMember(FWindowGroupState& InState, FWindowMembership& InMember) noexcept
{
	try
	{
		InState.RequireThread();
		InMember.Native.Restore();
	}
	catch (...)
	{
		ReportCleanupFailure();
	}
	Groups.erase(InMember.Window);
	InState.Members.erase(InMember.Window);
	InMember.Window = nullptr;
}

void ClearGroup(FWindowGroupState& InState) noexcept
{
	while (!InState.Members.empty())
	{
		auto Member = InState.Members.begin()->second;
		RemoveMember(InState, *Member);
	}
	Groups.erase(InState.Main);
	InState.Main = nullptr;
}
} // namespace

FWindowRegistration::FWindowRegistration(std::shared_ptr<FWindowMembership> InMembership)
    : Membership(std::move(InMembership))
{
}

FWindowRegistration::~FWindowRegistration()
{
	Reset();
}

FWindowRegistration::FWindowRegistration(FWindowRegistration&& InOther) noexcept = default;

FWindowRegistration& FWindowRegistration::operator=(FWindowRegistration&& InOther) noexcept
{
	if (this != &InOther)
	{
		Reset();
		Membership = std::move(InOther.Membership);
	}
	return *this;
}

void FWindowRegistration::Reset() noexcept
{
	if (Membership && Membership->Window)
	{
		if (auto State = Membership->Group.lock())
		{
			RemoveMember(*State, *Membership);
		}
	}
	Membership.reset();
}

bool FWindowRegistration::IsInputBlocked() const
{
	if (Membership && Membership->Window)
	{
		if (auto State = Membership->Group.lock())
		{
			State->RequireThread();
			return Membership->Native.IsBlocked();
		}
	}
	return false;
}

FWindowGroup::FWindowGroup(FWindow& InMain) : State(std::make_shared<FWindowGroupState>())
{
	CheckWindowGroupOwnership(InMain);
	FNativeWindowModality::ValidateMain(InMain.Surface());
	State->Main = &InMain;
	Groups.emplace(&InMain, State);
}

FWindowGroup::~FWindowGroup()
{
	ClearGroup(*State);
}

FWindowRegistration FWindowGroup::Register(FWindow& InWindow)
{
	State->RequireThread();
	if (!State->Main)
	{
		throw std::logic_error("Window group Main is no longer alive");
	}
	CheckWindowGroupOwnership(InWindow);
	auto Member = std::make_shared<FWindowMembership>(*State->Main, InWindow);
	Member->Group = State;
	State->Members.emplace(&InWindow, Member);
	try
	{
		Groups.emplace(&InWindow, State);
		if (State->bModal)
		{
			Member->Native.Block();
		}
	}
	catch (...)
	{
		RemoveMember(*State, *Member);
		throw;
	}
	return FWindowRegistration(std::move(Member));
}

void FWindowGroup::SetModalActive(bool bInActive)
{
	State->RequireThread();
	if (State->bModal == bInActive)
	{
		return;
	}
	if (!bInActive)
	{
		State->bModal = false;
		Synchronize();
		return;
	}
	std::vector<FWindowMembership*> Applied;
	Applied.reserve(State->Members.size());
	try
	{
		for (const auto& [Window, Member] : State->Members)
		{
			if (!Member->Native.IsBlocked())
			{
				Member->Native.Block();
				Applied.push_back(Member.get());
			}
		}
		if (State->Main && !State->Main->Minimized())
		{
			State->Main->Raise();
		}
	}
	catch (...)
	{
		for (auto* Member : Applied)
		{
			try
			{
				Member->Native.Restore();
			}
			catch (...)
			{
				ReportCleanupFailure();
			}
		}
		throw;
	}
	State->bModal = true;
}

void FWindowGroup::Synchronize()
{
	State->RequireThread();
	if (!State->Main)
	{
		throw std::logic_error("Window group Main is no longer alive");
	}
	const bool bMainMinimized = FNativeWindowModality::IsMainMinimized(State->Main->Surface());
	for (const auto& [Window, Member] : State->Members)
	{
		if (State->bModal)
		{
			Member->Native.Block();
		}
		Member->Native.Synchronize();
		if (!State->bModal && !bMainMinimized)
		{
			Member->Native.Restore();
		}
	}
}

bool FWindowGroup::IsInputBlocked(const FWindow& InWindow) const
{
	State->RequireThread();
	const auto It = State->Members.find(&InWindow);
	return It != State->Members.end() && It->second->Native.IsBlocked();
}

void CheckWindowGroupOwnership(const FWindow& InWindow)
{
	if (Groups.contains(&InWindow))
	{
		throw std::invalid_argument(
		    "Window is already registered; release its group membership before changing ownership");
	}
}

FWindow* GetWindowGroupInputOwner(const FWindow& InWindow)
{
	const auto It = Groups.find(&InWindow);
	if (It != Groups.end())
	{
		if (auto State = It->second.lock())
		{
			const auto Member = State->Members.find(&InWindow);
			if (Member != State->Members.end() && Member->second->Native.IsBlocked())
			{
				return State->Main;
			}
		}
	}
	return nullptr;
}

void RemoveWindowFromGroup(FWindow& InWindow) noexcept
{
	const auto It = Groups.find(&InWindow);
	if (It != Groups.end())
	{
		if (auto State = It->second.lock())
		{
			if (State->Main == &InWindow)
			{
				ClearGroup(*State);
			}
			else
			{
				auto Member = State->Members.at(&InWindow);
				RemoveMember(*State, *Member);
			}
		}
	}
}
} // namespace Hyperion
