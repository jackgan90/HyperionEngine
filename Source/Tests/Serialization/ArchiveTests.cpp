#include "Hyperion/Serialization/Archive.h"
#include "Support/BinaryFixtures.h"
#include "Support/TestSupport.h"
#include <array>
#include <iostream>
#include <limits>

namespace Hyperion
{
struct FArchiveChild
{
	std::string Name = "default";
	std::vector<float> Samples;
};

struct FArchiveFixture
{
	std::vector<FArchiveChild> Children;
	std::uint32_t Count = 7;
	std::array<FArchiveChild, 2> Slots;
};

template<> const FRecordDescriptor& RecordType<FArchiveChild>()
{
	static const auto Type = MakeRecord<FArchiveChild>(
	    "test.child", {Member("name", &FArchiveChild::Name), Member("samples", &FArchiveChild::Samples)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FArchiveFixture>()
{
	static const auto Type = MakeRecord<FArchiveFixture>(
	    "test.fixture", {Member("children", &FArchiveFixture::Children), Member("count", &FArchiveFixture::Count),
	                     Member("slots", &FArchiveFixture::Slots)});
	return Type;
}
} // namespace Hyperion

void CheckRecordEvolution();

namespace
{
using namespace Hyperion;
using namespace Hyperion::Test;

template<class T> void RejectArchive(T InWork)
{
	bool bRejected{};
	try
	{
		InWork();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

std::vector<std::byte> KnownBulkArchive()
{
	// Independently specified HYPA v2: [true, u16 bulk], 19 metadata bytes, one four-byte block at 59.
	return FixtureBytes("485950410200000013000000000000000100000000000000"
	                    "3b0000000000000004000000000000000502000000000104030000007531360000000001020304");
}

void CheckKnownMetadata()
{
	const auto Bytes = KnownBulkArchive();
	const auto Value = FArchiveNode(
	    FArchiveNode::FArray{FArchiveNode(true), FArchiveNode(FBulkData{EBulkElement::U16, FixtureBytes("01020304")})});
	HYP_CHECK(EncodeArchive(Value) == Bytes);
	HYP_CHECK(HashArchive(Value) == "0146b51f717ebf8442dc6594f9ac918448d609f44789654ea8a9995489901796");
	HYP_CHECK(EncodeArchive(DecodeArchive(Bytes)) == Bytes);
	HYP_CHECK(GetArchiveMetadataPrefixSize() == 24);
	HYP_CHECK(ProbeArchiveMetadataSize(std::span(Bytes).first(24), 63) == 59);
	const auto Metadata = DecodeArchiveMetadata(std::span(Bytes).first(59), 63);
	const auto& Values = std::get<FArchiveNode::FArray>(Metadata.Value);
	HYP_CHECK(Values.size() == 2 && std::get<bool>(Values[0].Value));
	HYP_CHECK(std::holds_alternative<std::monostate>(Values[1].Value));
	const auto Legacy = FixtureBytes("48595041010000000502000000000104030000007531360400000001020304");
	HYP_CHECK(EncodeArchive(DecodeArchive(Legacy)) == Bytes);
	RejectArchive(
	    [&]
	    {
		    ProbeArchiveMetadataSize(std::span(Legacy).first(24), Legacy.size());
	    });
	RejectArchive(
	    [&]
	    {
		    DecodeArchiveMetadata(Legacy, Legacy.size());
	    });
}

void CheckMalformedMetadata()
{
	const auto Good = KnownBulkArchive();

	struct FMutation
	{
		std::size_t Offset;
		unsigned Size;
		std::uint64_t Value;
	};

	const std::array Mutations{FMutation{0, 1, 0},
	                           FMutation{4, 4, 3},
	                           FMutation{20, 4, 1},
	                           FMutation{8, 8, 64},
	                           FMutation{8, 8, std::numeric_limits<std::uint64_t>::max()},
	                           FMutation{16, 4, 1000001},
	                           FMutation{16, 4, std::numeric_limits<std::uint32_t>::max()},
	                           FMutation{24, 8, 58},
	                           FMutation{24, 8, 60},
	                           FMutation{24, 8, std::numeric_limits<std::uint64_t>::max()},
	                           FMutation{32, 8, 3},
	                           FMutation{32, 8, 5},
	                           FMutation{55, 4, 1},
	                           FMutation{54, 1, '4'}};
	for (const auto Mutation : Mutations)
	{
		auto Bad = Good;
		WriteFixtureInteger(Bad, Mutation.Offset, Mutation.Size, Mutation.Value);
		if (Mutation.Offset < 24)
		{
			RejectArchive(
			    [&]
			    {
				    ProbeArchiveMetadataSize(std::span(Bad).first(24), 63);
			    });
		}
		RejectArchive(
		    [&]
		    {
			    DecodeArchive(Bad);
		    });
		RejectArchive(
		    [&]
		    {
			    DecodeArchiveMetadata(std::span(Bad).first(59), 63);
		    });
	}
	for (std::size_t Size = 0; Size < 59; ++Size)
	{
		if (Size < 24)
		{
			RejectArchive(
			    [&]
			    {
				    ProbeArchiveMetadataSize(std::span(Good).first(Size), 63);
			    });
		}
		RejectArchive(
		    [&]
		    {
			    DecodeArchiveMetadata(std::span(Good).first(Size), 63);
		    });
	}
	for (const auto Total : {std::size_t{58}, std::size_t{62}, std::size_t{64}})
	{
		RejectArchive(
		    [&]
		    {
			    DecodeArchiveMetadata(std::span(Good).first(59), Total);
		    });
	}
}

void CheckBulkMetadataBoundaries()
{
	auto Unaligned = KnownBulkArchive();
	Unaligned.pop_back();
	WriteFixtureInteger(Unaligned, 32, 8, 3);
	RejectArchive(
	    [&]
	    {
		    DecodeArchive(Unaligned);
	    });
	RejectArchive(
	    [&]
	    {
		    DecodeArchiveMetadata(std::span(Unaligned).first(59), 62);
	    });
	const auto TwoBlocks = EncodeArchive(
	    FArchiveNode(FArchiveNode::FArray{FArchiveNode(FBulkData{EBulkElement::U8, FixtureBytes("01")}),
	                                      FArchiveNode(FBulkData{EBulkElement::U8, FixtureBytes("02")})}));
	HYP_CHECK(TwoBlocks.size() == 85 && ReadFixtureInteger(TwoBlocks, 8, 8) == 27);
	HYP_CHECK(ProbeArchiveMetadataSize(std::span(TwoBlocks).first(24), 85) == 83);
	(void)DecodeArchiveMetadata(std::span(TwoBlocks).first(83), 85);
	for (const auto Offset : {std::uint64_t{83}, std::uint64_t{85}, std::numeric_limits<std::uint64_t>::max()})
	{
		auto Bad = TwoBlocks;
		WriteFixtureInteger(Bad, 40, 8, Offset);
		RejectArchive(
		    [&]
		    {
			    DecodeArchive(Bad);
		    });
		RejectArchive(
		    [&]
		    {
			    DecodeArchiveMetadata(std::span(Bad).first(83), 85);
		    });
	}
}

void CheckMetadataBudgets()
{
	const auto Bytes = KnownBulkArchive();
	FArchiveLimits Limits;
	Limits.MaxBytes = 63;
	Limits.MaxNodes = 3;
	Limits.MaxDepth = 1;
	HYP_CHECK(EncodeArchive(DecodeArchive(Bytes, Limits)) == Bytes);
	(void)DecodeArchiveMetadata(std::span(Bytes).first(59), 63, Limits);
	HYP_CHECK(ProbeArchiveMetadataSize(std::span(Bytes).first(24), 63, Limits) == 59);
	for (const auto Low : {FArchiveLimits{.MaxBytes = 62}, FArchiveLimits{.MaxNodes = 0}})
	{
		RejectArchive(
		    [&]
		    {
			    ProbeArchiveMetadataSize(std::span(Bytes).first(24), 63, Low);
		    });
	}
	const std::array LowLimits{FArchiveLimits{.MaxBytes = 62}, FArchiveLimits{.MaxAllocatedBytes = 1},
	                           FArchiveLimits{.MaxNodes = 2}, FArchiveLimits{.MaxDepth = 0}};
	for (const auto Low : LowLimits)
	{
		RejectArchive(
		    [&]
		    {
			    DecodeArchive(Bytes, Low);
		    });
		RejectArchive(
		    [&]
		    {
			    DecodeArchiveMetadata(std::span(Bytes).first(59), 63, Low);
		    });
	}
}
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		CheckKnownMetadata();
		CheckMalformedMetadata();
		CheckBulkMetadataBoundaries();
		CheckMetadataBudgets();
		CheckRecordEvolution();
		FArchiveFixture Original{{{"mesh", {1, 2, 3}}}, 42};
		Original.Slots[1].Name = "fixed element";
		auto Bytes = Serialize(Original);
		auto Restored = Deserialize<FArchiveFixture>(Bytes);
		HYP_CHECK(Restored.Count == 42 && Restored.Children[0].Samples == Original.Children[0].Samples);
		HYP_CHECK(Restored.Slots[1].Name == "fixed element");
		auto Node = WriteValue(Original);
		auto& Fields = std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Node.Value).at("fields").Value);
		Fields.erase("count");
		Fields.emplace("unknown", FArchiveNode(true));
		HYP_CHECK(ReadValue<FArchiveFixture>(Node).Count == 7);
		for (std::size_t Size : {std::size_t(0), std::size_t(7), Bytes.size() - 1})
		{
			bool bFailed = false;
			try
			{
				Restored = Deserialize<FArchiveFixture>(std::span(Bytes).first(Size));
			}
			catch (...)
			{
				bFailed = true;
			}
			HYP_CHECK(bFailed && Restored.Count == 42);
		}
		std::get<FArchiveNode::FObject>(Node.Value)["version"] = WriteValue(2u);
		bool bFailed = false;
		try
		{
			ReadValue<FArchiveFixture>(Node);
		}
		catch (...)
		{
			bFailed = true;
		}
		HYP_CHECK(bFailed);
		std::cout << "Reflected archive checks passed\n";
	}
	catch (const std::exception& InError)
	{
		std::cerr << InError.what() << '\n';
		return 1;
	}
}
