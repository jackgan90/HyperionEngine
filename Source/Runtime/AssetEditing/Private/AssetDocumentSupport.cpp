#include "Hyperion/AssetEditing/AssetDocument.h"
#include "Hyperion/Environment/SkyAsset.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/Model.h"
#include <algorithm>
#include <array>

namespace Hyperion
{
bool SupportsAssetDocument(const FRecordDescriptor& InType)
{
	static const std::array Types{&RecordType<FModelAsset>(), &RecordType<FMaterialAsset>(),
	                              &RecordType<FTextureAsset>(), &RecordType<FSkyAsset>()};
	return std::any_of(Types.begin(), Types.end(),
	                   [&](const FRecordDescriptor* InSupported)
	                   {
		                   return InType.Id == InSupported->Id && InType.CppType == InSupported->CppType;
	                   });
}
} // namespace Hyperion
