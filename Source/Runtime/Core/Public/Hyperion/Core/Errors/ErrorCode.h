#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace Hyperion
{
// Domain catalogs own the static spelling and lifetime of known identifiers.
class FErrorCodeId
{
public:
	explicit constexpr FErrorCodeId(std::string_view InName) : Name(InName)
	{
	}

	constexpr std::string_view GetName() const noexcept
	{
		return Name;
	}

	bool operator==(const FErrorCodeId&) const = default;

private:
	std::string_view Name;
};

// Owns unknown provider/protocol identifiers as well as known catalog identities.
class FErrorCode
{
public:
	FErrorCode(FErrorCodeId InId) : Name(InId.GetName())
	{
	}

	static FErrorCode FromExternal(std::string InName)
	{
		return FErrorCode(std::move(InName));
	}

	const std::string& GetName() const noexcept
	{
		return Name;
	}

	bool operator==(const FErrorCode&) const = default;

	bool operator==(FErrorCodeId InId) const noexcept
	{
		return Name == InId.GetName();
	}

private:
	explicit FErrorCode(std::string InName) : Name(std::move(InName))
	{
	}

	std::string Name;
};

class FCodedError : public std::runtime_error
{
public:
	FCodedError(FErrorCode InCode, std::string InMessage)
	    : std::runtime_error(std::move(InMessage)), Code(std::move(InCode))
	{
	}

	FErrorCode Code;
};
} // namespace Hyperion
