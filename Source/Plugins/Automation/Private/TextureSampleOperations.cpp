#include "AssetOperations.h"
#include <cstring>

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FTextureSampleRequest>()
{
	static const auto Type = MakeRecord<FTextureSampleRequest>(
	    "automation.texture.sample.request",
	    {Member("document", &FTextureSampleRequest::Document, {.bRequired = true}),
	     Member("generation", &FTextureSampleRequest::Generation, {.bRequired = true}),
	     Member("mip", &FTextureSampleRequest::Mip), Member("face", &FTextureSampleRequest::Face),
	     Member("x", &FTextureSampleRequest::X), Member("y", &FTextureSampleRequest::Y)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FTextureSample>()
{
	static const auto Type = MakeRecord<FTextureSample>(
	    "automation.texture.sample.result",
	    {Member("width", &FTextureSample::Width), Member("height", &FTextureSample::Height),
	     Member("mips", &FTextureSample::Mips), Member("faces", &FTextureSample::Faces),
	     Member("mipBytes", &FTextureSample::MipBytes), Member("rgba", &FTextureSample::Rgba)});
	return Type;
}

FTextureSample FAssetAutomation::TextureSample(const FTextureSampleRequest& InRequest)
{
	const auto Entry = Find(InRequest.Document);
	if (Entry->Document->Generation() != InRequest.Generation)
	{
		throw FAutomationError("stale_revision", "Texture changed; query current generation");
	}
	if (Entry->Document->Loaded().Header.TypeId != RecordType<FTextureAsset>().Id)
	{
		throw FAutomationError("unsupported_type", "A texture document is required");
	}
	const auto& Mips = std::get<FArchiveNode::FArray>(Entry->Document->Get("mips").Value);
	if (InRequest.Mip >= Mips.size())
	{
		throw std::invalid_argument("Mip out of range");
	}
	const auto& Mip =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Mips[InRequest.Mip].Value).at("fields").Value);
	FTextureSample Result;
	Result.Width = ReadValue<std::uint32_t>(Mip.at("width"));
	Result.Height = ReadValue<std::uint32_t>(Mip.at("height"));
	Result.Mips = static_cast<std::uint32_t>(Mips.size());
	Result.Faces = ReadValue<ETextureDimension>(Entry->Document->Get("dimension")) == ETextureDimension::Cube ? 6 : 1;
	if (InRequest.X >= Result.Width || InRequest.Y >= Result.Height || InRequest.Face >= Result.Faces)
	{
		throw std::invalid_argument("Texture coordinate or face out of range");
	}
	const auto Format = ReadValue<ETextureFormat>(Entry->Document->Get("format"));
	const auto Bytes = std::get<FBulkData>(Mip.at("bytes").Value).Data();
	Result.MipBytes = Bytes.size();
	const auto PixelBytes = TexturePixelBytes(Format);
	const auto Pixel = (std::size_t(InRequest.Face) * Result.Height + InRequest.Y) * Result.Width + InRequest.X;
	const auto Offset = Pixel * PixelBytes;
	if (Offset > Bytes.size() || PixelBytes > Bytes.size() - Offset)
	{
		throw std::runtime_error("Invalid pixel payload");
	}
	FMaterialTextureMip Single{1, 1};
	Single.Bytes.resize(PixelBytes);
	std::memcpy(Single.Bytes.data(), Bytes.data() + Offset, PixelBytes);
	Result.Rgba = ReadTexturePixel(Single, Format, 0);
	return Result;
}

void RegisterTextureSamples(FOperationCatalog& InCatalog, FAssetAutomation* InProvider)
{
	FOperationInfo Info;
	Info.Id = "texture.sample";
	Info.Owner = "automation-assets";
	Info.Summary = "Inspect texture mip metadata and one source pixel";
	Info.Description =
	    "Same source RGBA values as texture preview hover, before display conversion. Select mip/face/x/y. Returns "
	    "dimensions and mip count without copying bulk texture data. Cube faces are +X,-X,+Y,-Y,+Z,-Z.";
	Info.bReadOnly = true;
	Info.Effects = "Reads current texture draft.";
	Info.Completion = "Current Main snapshot.";
	Info.Unavailable = InProvider ? "" : "Asset workspace unavailable.";
	const FTextureSampleRequest Example{"document-from-open", 1};
	Info.Example = WriteRecordWire(RecordType<FTextureSampleRequest>(), &Example);
	InCatalog.Register(MakeOperation<FTextureSampleRequest, FTextureSample>(Info,
	                                                                        [InProvider](const auto& InRequest)
	                                                                        {
		                                                                        return InProvider->TextureSample(
		                                                                            InRequest);
	                                                                        }));
}
} // namespace Hyperion
