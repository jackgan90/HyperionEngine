#pragma once
#if !HYP_BUILD_TESTING
#error Acceptance transitions are only available in test-enabled builds
#endif
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace Hyperion
{
#define HYP_ACCEPTANCE_BEGIN(Name)                                                                                     \
	enum class E##Name##State                                                                                          \
	{
#define HYP_ACCEPTANCE_STATE(Name) Name,
#define HYP_ACCEPTANCE_END(Name)                                                                                       \
	}                                                                                                                  \
	;
#define HYP_ACCEPTANCE_ASSET_STATE(Name, Stage, Window) HYP_ACCEPTANCE_STATE(Name)
#include "AcceptanceStates.inl"
#undef HYP_ACCEPTANCE_ASSET_STATE
#undef HYP_ACCEPTANCE_BEGIN
#undef HYP_ACCEPTANCE_STATE
#undef HYP_ACCEPTANCE_END

#define HYP_ACCEPTANCE_BEGIN(Name)                                                                                     \
	inline std::string_view GetAcceptanceStateName(E##Name##State InState)                                             \
	{                                                                                                                  \
		switch (InState)                                                                                               \
		{
#define HYP_ACCEPTANCE_STATE(Name)                                                                                     \
	case decltype(InState)::Name:                                                                                      \
		return #Name;
#define HYP_ACCEPTANCE_END(Name)                                                                                       \
	}                                                                                                                  \
	throw std::logic_error("Unknown " #Name " acceptance state");                                                      \
	}
#define HYP_ACCEPTANCE_ASSET_STATE(Name, Stage, Window) HYP_ACCEPTANCE_STATE(Name)
#include "AcceptanceStates.inl"
#undef HYP_ACCEPTANCE_ASSET_STATE
#undef HYP_ACCEPTANCE_BEGIN
#undef HYP_ACCEPTANCE_STATE
#undef HYP_ACCEPTANCE_END

template<typename TState> class TAcceptanceState
{
	static_assert(std::is_enum_v<TState>, "Acceptance execution states must be typed enums");

public:
	TState GetState() const
	{
		return State;
	}

	bool Is(TState InState) const
	{
		return State == InState;
	}

	bool IsAny(std::initializer_list<TState> InStates) const
	{
		return std::find(InStates.begin(), InStates.end(), State) != InStates.end();
	}

	void TransitionTo(TState InState)
	{
		State = InState;
	}

	std::string Name() const
	{
		return std::string(GetAcceptanceStateName(State));
	}

private:
	TState State{};
};
} // namespace Hyperion
