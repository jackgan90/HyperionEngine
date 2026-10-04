#include "Hyperion/AssetImport/ImageImport.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
void RegisterImageImporter(FAssetImportService& InImports)
{
	InImports.Register({ImageImporterId,
	                    1,
	                    &RecordType<FTextureAsset>(),
	                    {".png", ".jpg", ".jpeg"},
	                    [](FAssetImportContext& InContext) -> std::shared_ptr<void>
	                    {
		                    auto Image = DecodeImage(*InContext.Bytes);
		                    return std::make_shared<FTextureAsset>(BuildTextureAsset(
		                        PathToUtf8(InContext.Path.stem()),
		                        InContext.Settings.TextureEncoding.value_or(EMaterialTextureEncoding::Srgb),
		                        {Image.Width, Image.Height, std::move(Image.Rgba)}));
	                    }});
}
} // namespace Hyperion
