#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/Core/ContentHash.h"
#include "Support/BinaryFixtures.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

std::vector<std::byte> PackDocument(const FAssetDocument& InDocument)
{
	const auto Payload = EncodeArchive(
	    FArchiveNode(FArchiveNode::FObject{{"header", WriteValue(InDocument.Header)}, {"object", InDocument.Object}}));
	auto Bytes = Test::FixtureBytes("4841535401000000");
	for (unsigned Index = 0; Index < 8; ++Index)
	{
		Bytes.push_back(std::byte((std::uint64_t(Payload.size()) >> (Index * 8)) & 255));
	}
	for (const auto Character : ContentHash(Payload))
	{
		Bytes.push_back(std::byte(Character));
	}
	Bytes.insert(Bytes.end(), Payload.begin(), Payload.end());
	return Bytes;
}

void CheckStoredDependencyPaths()
{
	const FAssetRef Reference{"", "child.hasset", "unknown.asset", ""};
	const auto RefNode = WriteValue(Reference);
	const FArchiveNode Unknown(FArchiveNode::FObject{{"type", WriteValue(std::string("unknown.nested"))},
	                                                 {"version", WriteValue(std::string("opaque"))},
	                                                 {"fields", FArchiveNode(FArchiveNode::FObject{{"ref", RefNode}})},
	                                                 {"ignored", RefNode}});
	const FArchiveNode Map(FArchiveNode::FObject{{"type", WriteValue(std::string("ordinary.map"))},
	                                             {"fields", FArchiveNode(FArchiveNode::FObject{{"ref", RefNode}})},
	                                             {"other", RefNode}});
	const FArchiveNode Root(FArchiveNode::FObject{
	    {"type", WriteValue(std::string("unknown.root"))},
	    {"version", WriteValue(1u)},
	    {"fields", FArchiveNode(FArchiveNode::FObject{
	                   {"nested", Unknown}, {"map", Map}, {"array", FArchiveNode(FArchiveNode::FArray{Unknown})}})}});
	const auto Bytes = EncodeArchive(Root);
	const auto Legacy = DecodeAsset(Bytes);
	HYP_CHECK(Legacy.bLegacy && Legacy.Header.TypeId == "unknown.root" && Legacy.Header.SchemaVersion == 1);
	const std::vector<FAssetDependency> Expected{{"array[0].ref", Reference},
	                                             {"map[fields][ref]", Reference},
	                                             {"map[other]", Reference},
	                                             {"nested.ref", Reference}};
	HYP_CHECK(Legacy.Header.Dependencies == Expected);
	HYP_CHECK(EncodeArchive(Legacy.Object) == Bytes);
	HYP_CHECK(Legacy.Header.Revision == HashArchive(Root));
	const auto Native = DecodeAsset(PackDocument(Legacy));
	HYP_CHECK(!Native.bLegacy && Native.Header.Dependencies == Expected);
	HYP_CHECK(EncodeArchive(Native.Object) == Bytes);
}

void CheckStoredReferenceValidation()
{
	auto Reference = WriteValue(FAssetRef{"", "child.hasset", "unknown.asset", ""});
	std::get<FArchiveNode::FObject>(Reference.Value)["version"] = WriteValue(std::string("opaque"));
	const FArchiveNode Root(FArchiveNode::FObject{{"type", WriteValue(std::string("unknown.root"))},
	                                              {"version", WriteValue(1u)},
	                                              {"fields", FArchiveNode(FArchiveNode::FObject{{"ref", Reference}})}});
	bool bRejected{};
	try
	{
		(void)DecodeAsset(EncodeArchive(Root));
	}
	catch (const std::exception& Error)
	{
		bRejected = std::string_view(Error.what()).find(RecordType<FAssetRef>().Id) != std::string_view::npos;
	}
	HYP_CHECK(bRejected);
}
} // namespace

void CheckNativeEnvelope()
{
	CheckStoredDependencyPaths();
	CheckStoredReferenceValidation();
}
