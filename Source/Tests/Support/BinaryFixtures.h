#pragma once
#include "Support/TestSupport.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Hyperion::Test
{
inline std::vector<std::byte> FixtureBytes(std::string_view InHex)
{
	HYP_CHECK(InHex.size() % 2 == 0);
	const auto Nibble = [](char InCharacter) -> unsigned
	{
		if (InCharacter >= '0' && InCharacter <= '9')
		{
			return InCharacter - '0';
		}
		HYP_CHECK(InCharacter >= 'a' && InCharacter <= 'f');
		return InCharacter - 'a' + 10;
	};
	std::vector<std::byte> Result;
	for (std::size_t Index = 0; Index < InHex.size(); Index += 2)
	{
		Result.push_back(std::byte((Nibble(InHex[Index]) << 4) | Nibble(InHex[Index + 1])));
	}
	return Result;
}

inline std::uint64_t ReadFixtureInteger(std::span<const std::byte> InBytes, std::size_t InOffset, unsigned InSize)
{
	HYP_CHECK(InSize <= 8 && InOffset <= InBytes.size() && InSize <= InBytes.size() - InOffset);
	std::uint64_t Result{};
	for (unsigned Index = 0; Index < InSize; ++Index)
	{
		Result |= std::uint64_t(std::to_integer<unsigned>(InBytes[InOffset + Index])) << (Index * 8);
	}
	return Result;
}

inline void WriteFixtureInteger(std::span<std::byte> OutBytes, std::size_t InOffset, unsigned InSize,
                                std::uint64_t InValue)
{
	HYP_CHECK(InSize <= 8 && InOffset <= OutBytes.size() && InSize <= OutBytes.size() - InOffset);
	for (unsigned Index = 0; Index < InSize; ++Index)
	{
		OutBytes[InOffset + Index] = std::byte((InValue >> (Index * 8)) & 255);
	}
}
} // namespace Hyperion::Test
