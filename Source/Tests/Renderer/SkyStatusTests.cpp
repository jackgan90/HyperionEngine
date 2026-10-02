#include "Hyperion/Reflection/Json.h"
#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/Renderer/SceneLightControls.h"
#include "Support/TestSupport.h"
#include <array>
#include <iostream>

using namespace Hyperion;

namespace
{
void CheckWire()
{
	// Independent protocol fixtures: do not derive these strings from the production mapping.
	constexpr std::array States{ESceneSkyState::None,      ESceneSkyState::Unrequested, ESceneSkyState::Loading,
	                            ESceneSkyState::Uploading, ESceneSkyState::Ready,       ESceneSkyState::Failed};
	constexpr std::array Names{"", "unrequested", "loading", "uploading", "ready", "failed"};
	for (std::size_t Index = 0; Index < States.size(); ++Index)
	{
		const FSceneSkyStatus Status{States[Index], "Preparing is an error"};
		const FArchiveNode Expected(FArchiveNode::FObject{{"state", WriteValue(std::string(Names[Index]))},
		                                                  {"error", WriteValue(Status.Error)}});
		HYP_CHECK(WriteJson(WriteRecordWire(RecordType<FSceneSkyStatus>(), &Status)) == WriteJson(Expected));
		const auto Restored =
		    std::static_pointer_cast<FSceneSkyStatus>(ReadRecordWire(RecordType<FSceneSkyStatus>(), Expected));
		HYP_CHECK(Restored->State == Status.State && Restored->Error == Status.Error);
		const auto Archive = ReadValue<FSceneSkyStatus>(WriteValue(Status));
		HYP_CHECK(Archive.State == Status.State && Archive.Error == Status.Error);
	}
	for (const auto* Invalid : {R"({"state":"unexpected"})", R"({"state":5})"})
	{
		bool bRejected{};
		try
		{
			ReadRecordWire(RecordType<FSceneSkyStatus>(), ParseJson(Invalid));
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
	}
}

void CheckSchemaAndDefaults()
{
	const auto Expected = ParseJson(
	    R"({"$schema":"https://json-schema.org/draft/2020-12/schema","additionalProperties":false,"properties":{"error":{"default":"","type":"string"},"state":{"default":"","type":"string"}},"required":[],"type":"object","x-hyperion-type":"scene.sky.status","x-hyperion-version":1})");
	HYP_CHECK(WriteJson(RecordWireSchema(RecordType<FSceneSkyStatus>())) == WriteJson(Expected));
	FSceneLightingInfo Info;
	Info.Lights.emplace_back();
	const auto Wire = WriteRecordWire(RecordType<FSceneLightingInfo>(), &Info);
	const auto& Fields = std::get<FArchiveNode::FObject>(Wire.Value);
	const auto& Light =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FArray>(Fields.at("lights").Value).front().Value);
	HYP_CHECK(WriteJson(Light.at("asset")) == R"({"error":"","state":""})");
	for (const std::string Error : {"Preparing is an error", "Failed: collision", ""})
	{
		const FSceneSkyStatus Failed{ESceneSkyState::Failed, Error};
		HYP_CHECK(FormatSceneSkyStatus(Failed) == "Failed: " + Error);
	}
}
} // namespace

int main()
{
	try
	{
		CheckWire();
		CheckSchemaAndDefaults();
		std::cout << "PASS: sky string wire/schema fixtures and typed failures\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
