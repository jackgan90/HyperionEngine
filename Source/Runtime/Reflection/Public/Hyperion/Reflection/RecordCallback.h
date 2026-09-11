#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace Hyperion
{
// Copies preserve the callable's binding identity; replacing a callback creates a new identity.
template<class T> class TRecordCallback;

template<class R, class... A> class TRecordCallback<R(A...)>
{
public:
	TRecordCallback() = default;

	template<class F>
	    requires(!std::is_same_v<std::remove_cvref_t<F>, TRecordCallback>)
	TRecordCallback(F&& InFunction) : Function(std::forward<F>(InFunction)), Identity(std::make_shared<const int>(0))
	{
	}

	R operator()(A... InArguments) const
	{
		return Function(std::forward<A>(InArguments)...);
	}

	explicit operator bool() const
	{
		return static_cast<bool>(Function);
	}

	bool operator==(const TRecordCallback& InOther) const
	{
		return Identity == InOther.Identity;
	}

private:
	std::function<R(A...)> Function;
	std::shared_ptr<const void> Identity;
};
} // namespace Hyperion
