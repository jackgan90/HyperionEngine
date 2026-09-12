#include "Hyperion/AssetImport/AssetImportService.h"
#include <algorithm>

namespace Hyperion
{
FAssetRef FAssetImportContext::Emit(FAssetImportProduct InProduct)
{
	Cancellation.Check();
	if (Products.size() >= 4096 || InProduct.Key.empty() ||
	    InProduct.Key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.") !=
	        std::string::npos ||
	    !InProduct.Type || !InProduct.Object ||
	    std::any_of(Products.begin(), Products.end(),
	                [&](const auto& InExisting)
	                {
		                return InExisting.Key == InProduct.Key;
	                }))
	{
		throw std::invalid_argument("Invalid, duplicate or excessive named import product");
	}
	ValidateRecordDescriptor(*InProduct.Type);
	if (InProduct.Type->Validate)
	{
		InProduct.Type->Validate(InProduct.Object.get());
	}
	FAssetRef Reference{"", "@" + InProduct.Key, InProduct.Type->Id, ""};
	Products.push_back(std::move(InProduct));
	return Reference;
}
} // namespace Hyperion
