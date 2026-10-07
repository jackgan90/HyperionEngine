#include "Hyperion/AssetImport/AssetSourceJson.h"
#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/Serialization/Archive.h"
#include "Support/BinaryFixtures.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <limits>

void CheckBulkGuiControls();

namespace
{
using namespace Hyperion;
using namespace Hyperion::Test;

template<class T> void Reject(T InWork)
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

std::vector<std::byte> KnownArchive(std::string_view InName, std::span<const std::byte> InPayload, bool bInLegacy)
{
	// Independent HYPA fixtures: one bulk node, no record envelope. No production mapping/encoder is used.
	auto Bytes = FixtureBytes(bInLegacy ? "4859504101000000" : "4859504102000000");
	const std::size_t NodeOffset = bInLegacy ? 8 : 40;
	const std::size_t PayloadOffset = NodeOffset + 9 + InName.size();
	Bytes.resize(PayloadOffset);
	if (!bInLegacy)
	{
		WriteFixtureInteger(Bytes, 8, 8, 9 + InName.size());
		WriteFixtureInteger(Bytes, 16, 4, 1);
		WriteFixtureInteger(Bytes, 24, 8, PayloadOffset);
		WriteFixtureInteger(Bytes, 32, 8, InPayload.size());
	}
	Bytes[NodeOffset] = std::byte{4};
	WriteFixtureInteger(Bytes, NodeOffset + 1, 4, InName.size());
	for (std::size_t Index = 0; Index < InName.size(); ++Index)
	{
		Bytes[NodeOffset + 5 + Index] = std::byte(InName[Index]);
	}
	WriteFixtureInteger(Bytes, PayloadOffset - 4, 4, bInLegacy ? InPayload.size() : 0);
	Bytes.insert(Bytes.end(), InPayload.begin(), InPayload.end());
	return Bytes;
}

template<class T>
void CheckType(EBulkElement InElement, std::string_view InName, std::string_view InHex, const std::vector<T>& InValues,
               std::string_view InJson, std::string_view InWireJson)
{
	const auto& Info = GetBulkElementInfo(InElement);
	HYP_CHECK(Info.WireName == InName && Info.ByteSize == sizeof(T));
	HYP_CHECK(ParseBulkElement(InName) == InElement && BulkElement<T>() == InElement);
	VisitBulkElement(InElement,
	                 []<class V>()
	                 {
		                 HYP_CHECK((std::is_same_v<T, V>));
	                 });
	const auto Payload = FixtureBytes(InHex);
	const auto Node = WriteBulk(InValues);
	HYP_CHECK(std::get<FBulkData>(Node.Value).Element == InElement);
	HYP_CHECK(std::get<FBulkData>(Node.Value).Bytes == Payload);
	const auto Bytes = KnownArchive(InName, Payload, false);
	HYP_CHECK(EncodeArchive(Node) == Bytes);
	HYP_CHECK(ReadBulk<std::vector<T>>(DecodeArchive(Bytes)) == InValues);
	HYP_CHECK(ReadBulk<std::vector<T>>(DecodeArchive(KnownArchive(InName, Payload, true))) == InValues);
	auto Enclosed = FixtureBytes("0000000000000000");
	Enclosed.insert(Enclosed.end(), Bytes.begin(), Bytes.end());
	const auto Owner = std::make_shared<const std::vector<std::byte>>(std::move(Enclosed));
	const auto Shared = DecodeArchive(Owner, 8);
	const auto& Bulk = std::get<FBulkData>(Shared.Value);
	HYP_CHECK(Bulk.Storage == Owner && Bulk.Bytes.empty());
	HYP_CHECK(Bulk.Offset == 8 + Bytes.size() - Payload.size() && Bulk.Size == Payload.size());
	HYP_CHECK(ReadBulk<std::vector<T>>(Shared) == InValues);
	const auto Metadata = DecodeArchiveMetadata(std::span(Bytes).first(Bytes.size() - Payload.size()), Bytes.size());
	HYP_CHECK(std::holds_alternative<std::monostate>(Metadata.Value));
	const auto& Shape = RecordValueShape<std::vector<T>>();
	const auto Wire = WriteValueWire(Shape, Node);
	HYP_CHECK(WriteJson(Wire) == InWireJson);
	HYP_CHECK(ReadBulk<std::vector<T>>(ReadValueWire(Shape, ParseJson(InWireJson))) == InValues);
	const auto Json = ParseJson(EncodeAssetSourceJson(Node));
	const auto Expected = ParseJson("{\"$bulk\":\"" + std::string(InName) + "\",\"data\":" + std::string(InJson) + "}");
	HYP_CHECK(EqualInspectionValue(Json, Expected));
	const auto Empty = WriteBulk(std::vector<T>{});
	HYP_CHECK(EncodeArchive(Empty) == KnownArchive(InName, {}, false));
	HYP_CHECK(ReadBulk<std::vector<T>>(DecodeArchive(EncodeArchive(Empty))).empty());
	HYP_CHECK(WriteJson(WriteValueWire(Shape, Empty)) == "[]");
	HYP_CHECK(ReadBulk<std::vector<T>>(ReadValueWire(Shape, ParseJson("[]"))).empty());
	if constexpr (sizeof(T) > 1)
	{
		const auto Bad = KnownArchive(InName, FixtureBytes("00"), false);
		Reject(
		    [&]
		    {
			    DecodeArchive(Bad);
		    });
		Reject(
		    [&]
		    {
			    DecodeArchiveMetadata(std::span(Bad).first(Bad.size() - 1), Bad.size());
		    });
		Reject(
		    [&]
		    {
			    DecodeArchive(KnownArchive(InName, FixtureBytes("00"), true));
		    });
	}
}

void CheckSupportedTypes()
{
	HYP_CHECK(BulkElements.size() == 10);
	CheckType<std::uint8_t>(EBulkElement::U8, "u8", "00ff", {0, 255}, "[0,255]", "[0,255]");
	CheckType<std::int8_t>(EBulkElement::I8, "i8", "807f", {-128, 127}, "[-128,127]", "[-128,127]");
	CheckType<std::uint16_t>(EBulkElement::U16, "u16", "0000ffff", {0, 65535}, "[0,65535]", "[0,65535]");
	CheckType<std::int16_t>(EBulkElement::I16, "i16", "0080ff7f", {-32768, 32767}, "[-32768,32767]", "[-32768,32767]");
	CheckType<std::uint32_t>(EBulkElement::U32, "u32", "00000000ffffffff", {0, 4294967295u}, "[0,4294967295]",
	                         "[0,4294967295]");
	CheckType<std::int32_t>(EBulkElement::I32, "i32", "00000080ffffff7f", {-2147483647 - 1, 2147483647},
	                        "[-2147483648,2147483647]", "[-2147483648,2147483647]");
	CheckType<std::uint64_t>(EBulkElement::U64, "u64", "0000000000000000ffffffffffffffff", {0, 18446744073709551615ull},
	                         "[0,18446744073709551615]", "[\"0\",\"18446744073709551615\"]");
	CheckType<std::int64_t>(EBulkElement::I64, "i64", "0000000000000080ffffffffffffff7f",
	                        {-9223372036854775807ll - 1, 9223372036854775807ll},
	                        "[-9223372036854775808,9223372036854775807]",
	                        "[\"-9223372036854775808\",\"9223372036854775807\"]");
	CheckType<float>(EBulkElement::F32, "f32", "0000a03f000020c0", {1.25f, -2.5f}, "[1.25,-2.5]", "[1.25,-2.5]");
	CheckType<double>(EBulkElement::F64, "f64", "000000000000f43f00000000000004c0", {1.25, -2.5}, "[1.25,-2.5]",
	                  "[1.25,-2.5]");
	static_assert(BulkElement<std::byte>() == EBulkElement::U8);
	static_assert(BulkElement<unsigned char>() == EBulkElement::U8);
	static_assert(BulkElement<signed char>() == EBulkElement::I8);
	const std::vector<std::byte> Bytes{std::byte{0}, std::byte{255}};
	HYP_CHECK(Serialize(Bytes) == KnownArchive("u8", Bytes, false));
	HYP_CHECK(Deserialize<std::vector<std::byte>>(Serialize(Bytes)) == Bytes);
}

void CheckInvalidElements()
{
	for (const auto Name : {"", "u128", "U8", "u08", "f16", "u8 "})
	{
		HYP_CHECK(!ParseBulkElement(Name));
		for (const bool bLegacy : {false, true})
		{
			const auto Bad = KnownArchive(Name, {}, bLegacy);
			Reject(
			    [&]
			    {
				    DecodeArchive(Bad);
			    });
			if (!bLegacy)
			{
				Reject(
				    [&]
				    {
					    DecodeArchiveMetadata(Bad, Bad.size());
				    });
			}
		}
	}
	for (const auto Element : {EBulkElement::Invalid, static_cast<EBulkElement>(255)})
	{
		HYP_CHECK(!FindBulkElementInfo(Element));
		Reject(
		    [&]
		    {
			    GetBulkElementInfo(Element);
		    });
		bool bVisited{};
		Reject(
		    [&]
		    {
			    VisitBulkElement(Element,
			                     [&]<class T>()
			                     {
				                     bVisited = true;
			                     });
		    });
		HYP_CHECK(!bVisited);
		const FArchiveNode Node(FBulkData{Element});
		Reject(
		    [&]
		    {
			    EncodeArchive(Node);
		    });
		Reject(
		    [&]
		    {
			    EncodeAssetSourceJson(Node);
		    });
		Reject(
		    [&]
		    {
			    WriteValueWire(RecordValueShape<std::vector<float>>(), Node);
		    });
	}
}

void CheckInvalidValues()
{
	const auto Node = WriteBulk(std::vector<std::uint32_t>{1, 2});
	Reject(
	    [&]
	    {
		    ReadBulk<std::vector<float>>(Node);
	    });
	Reject(
	    [&]
	    {
		    ReadBulk<std::array<std::uint32_t, 1>>(Node);
	    });
	Reject(
	    []
	    {
		    WriteBulk(std::vector<float>{std::numeric_limits<float>::infinity()});
	    });
	const FArchiveNode NonFinite(FBulkData{EBulkElement::F64, FixtureBytes("000000000000f87f")});
	Reject(
	    [&]
	    {
		    ReadBulk<std::vector<double>>(NonFinite);
	    });
	Reject(
	    [&]
	    {
		    WriteValueWire(RecordValueShape<std::vector<double>>(), NonFinite);
	    });
	Reject(
	    [&]
	    {
		    EncodeAssetSourceJson(NonFinite);
	    });
	const FArchiveNode Misaligned(FBulkData{EBulkElement::U32, FixtureBytes("00")});
	Reject(
	    [&]
	    {
		    ReadBulk<std::vector<std::uint32_t>>(Misaligned);
	    });
	Reject(
	    [&]
	    {
		    WriteValueWire(RecordValueShape<std::vector<std::uint32_t>>(), Misaligned);
	    });
	Reject(
	    [&]
	    {
		    EncodeAssetSourceJson(Misaligned);
	    });
	Reject(
	    []
	    {
		    ReadValueWire(RecordValueShape<std::vector<std::uint8_t>>(), ParseJson("[256]"));
	    });
	Reject(
	    []
	    {
		    ReadValueWire(RecordValueShape<std::vector<std::int8_t>>(), ParseJson("[-129]"));
	    });
	Reject(
	    []
	    {
		    ReadValueWire(RecordValueShape<std::vector<std::uint64_t>>(), ParseJson("[1]"));
	    });
}
} // namespace

int main()
{
	try
	{
		CheckSupportedTypes();
		CheckInvalidElements();
		CheckInvalidValues();
		CheckBulkGuiControls();
		std::cout << "Bulk element archive, wire, source JSON, GUI and rejection contracts passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
