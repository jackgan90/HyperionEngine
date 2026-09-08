#pragma once
#include "Hyperion/Materials/MaterialParameters.h"

namespace Hyperion
{
// Owned value-tree storage only; externally shared texture/read-buffer payloads are accounted by their owners.
std::size_t MaterialValueStorageBytes(const FMaterialValue& InValue);

// Publication is immutable: copying a scope shares its entire value tree. A replacement validates once.
class FMaterialInputValues
{
public:
	FMaterialInputValues() = default;
	FMaterialInputValues(FMaterialParameterValues InValues);

	FMaterialInputValues(std::initializer_list<FMaterialParameterEntry> InValues)
	    : FMaterialInputValues(FMaterialParameterValues(InValues))
	{
	}

	const FMaterialParameterValues& Get() const;

	bool operator==(const FMaterialInputValues& InOther) const
	{
		return Storage == InOther.Storage || Get() == InOther.Get();
	}

	const void* GetIdentity() const
	{
		return Storage.get();
	}

	const std::shared_ptr<const FMaterialParameterValues>& Share() const
	{
		return Storage;
	}

	std::size_t GetStorageBytes() const
	{
		return StorageBytes;
	}

private:
	std::shared_ptr<const FMaterialParameterValues> Storage;
	std::size_t StorageBytes{};
};
} // namespace Hyperion
