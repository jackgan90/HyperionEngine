#include "Hyperion/DerivedDataCache/DerivedDataCache.h"
#include <algorithm>
#include <stdexcept>
#include <windows.h>

#include <bcrypt.h>

namespace Hyperion
{
std::string DerivedDataDigest(std::string_view InBytes)
{
	BCRYPT_ALG_HANDLE Algorithm{};
	BCRYPT_HASH_HANDLE Hash{};
	if (BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
	{
		throw std::runtime_error("SHA256 provider");
	}

	struct FGuard
	{
		BCRYPT_ALG_HANDLE Algorithm;
		BCRYPT_HASH_HANDLE& Hash;

		~FGuard()
		{
			if (Hash)
			{
				BCryptDestroyHash(Hash);
			}
			BCryptCloseAlgorithmProvider(Algorithm, 0);
		}
	} Guard{Algorithm, Hash};

	if (BCryptCreateHash(Algorithm, &Hash, nullptr, 0, nullptr, 0, 0) < 0)
	{
		throw std::runtime_error("SHA256 initialization");
	}
	std::size_t Offset{};
	while (Offset < InBytes.size())
	{
		const auto Size = static_cast<ULONG>(std::min<std::size_t>(InBytes.size() - Offset, 1024 * 1024));
		if (BCryptHashData(Hash, reinterpret_cast<PUCHAR>(const_cast<char*>(InBytes.data() + Offset)), Size, 0) < 0)
		{
			throw std::runtime_error("SHA256 update");
		}
		Offset += Size;
	}
	unsigned char Digest[32]{};
	if (BCryptFinishHash(Hash, Digest, 32, 0) < 0)
	{
		throw std::runtime_error("SHA256 finish");
	}
	constexpr std::string_view Hex = "0123456789abcdef";
	std::string Result;
	for (const auto Byte : Digest)
	{
		Result += Hex[Byte >> 4];
		Result += Hex[Byte & 15];
	}
	return Result;
}
} // namespace Hyperion
