#pragma once
#include "Hyperion/Assets/AssetService.h"

namespace Hyperion
{
// Runtime GPU fixtures deliberately reject every source-format read.
class FNativeOnlyFileSystem final : public IFileSystem
{
public:
	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override
	{
		if (InPath.extension() != ".hasset")
		{
			++DeniedReads;
			throw std::runtime_error("Source reads disabled during native runtime test: " + InPath.generic_string());
		}
		return Local.Read(InPath, InLimit);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		Local.WriteAtomic(InPath, InBytes);
	}

	std::atomic_uint64_t DeniedReads{};

private:
	FLocalFileSystem Local;
};
} // namespace Hyperion
