#include "Hyperion/Core/ContentHash.h"
#include <array>
#include <limits>
#include <stdexcept>
#include <windows.h>

#include <bcrypt.h>

namespace Hyperion
{
namespace
{
std::string Hex(std::span<const unsigned char> InBytes)
{
	constexpr char Digits[] = "0123456789abcdef";
	std::string Result;
	Result.reserve(InBytes.size() * 2);
	for (const auto Byte : InBytes)
	{
		Result.push_back(Digits[Byte >> 4]);
		Result.push_back(Digits[Byte & 15]);
	}
	return Result;
}
} // namespace

std::string ContentHash(std::span<const std::byte> InBytes)
{
	if (InBytes.size() > std::numeric_limits<ULONG>::max())
	{
		throw std::runtime_error("Content hash input exceeds supported size");
	}
	std::array<unsigned char, 32> Digest{};
	const auto Status = BCryptHash(
	    BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<std::byte*>(InBytes.data())),
	    static_cast<ULONG>(InBytes.size()), Digest.data(), static_cast<ULONG>(Digest.size()));
	if (Status < 0)
	{
		throw std::runtime_error("Content SHA-256 failed");
	}
	return Hex(Digest);
}

std::string ContentHashParts(std::span<const std::span<const std::byte>> InParts)
{
	BCRYPT_HASH_HANDLE Handle{};
	if (BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &Handle, nullptr, 0, nullptr, 0, 0) < 0)
	{
		throw std::runtime_error("Cannot create content hash");
	}

	struct FHashOwner
	{
		BCRYPT_HASH_HANDLE Handle;

		~FHashOwner()
		{
			BCryptDestroyHash(Handle);
		}
	} Owner{Handle};

	for (const auto Part : InParts)
	{
		if (Part.size() > std::numeric_limits<ULONG>::max() ||
		    BCryptHashData(Handle, reinterpret_cast<PUCHAR>(const_cast<std::byte*>(Part.data())),
		                   static_cast<ULONG>(Part.size()), 0) < 0)
		{
			throw std::runtime_error("Content hash update failed");
		}
	}
	std::array<unsigned char, 32> Digest{};
	if (BCryptFinishHash(Handle, Digest.data(), static_cast<ULONG>(Digest.size()), 0) < 0)
	{
		throw std::runtime_error("Content hash finalization failed");
	}
	return Hex(Digest);
}

std::string CreateIdentifier()
{
	std::array<unsigned char, 16> Bytes{};
	if (BCryptGenRandom(nullptr, Bytes.data(), static_cast<ULONG>(Bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
	{
		throw std::runtime_error("Asset identity generation failed");
	}
	return Hex(Bytes);
}
} // namespace Hyperion
