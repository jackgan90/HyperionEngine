#include "Hyperion/Serialization/Archive.h"
#include "Support/TestSupport.h"
#include <array>
#include <iostream>

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

int main()
{
	using namespace Hyperion;
	try
	{
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
