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
	HYP_CHECK(WriteJson(Light.at("type")) == R"("")");
	for (const std::string Error : {"Preparing is an error", "Failed: collision", ""})
	{
		const FSceneSkyStatus Failed{ESceneSkyState::Failed, Error};
		HYP_CHECK(FormatSceneSkyStatus(Failed) == "Failed: " + Error);
	}
}

void CheckDiagnosticWire()
{
	const auto& Type = RecordType<FSceneLightDiagnostic>();
	for (const std::string Token :
	     {R"("directional")", R"("sky")", R"("")", R"("other")", R"("Sky")", R"(" sky")", R"("sky\u0000tail")"})
	{
		const auto Expected = ParseJson("{\"type\":" + Token + "}");
		const auto Restored = std::static_pointer_cast<FSceneLightDiagnostic>(ReadRecordWire(Type, Expected));
		const auto Name = ReadValue<std::string>(ParseJson(Token));
		HYP_CHECK(Restored->Type.WireName() == Name);
		const auto Wire = WriteRecordWire(Type, Restored.get());
		HYP_CHECK(WriteJson(std::get<FArchiveNode::FObject>(Wire.Value).at("type")) == WriteJson(ParseJson(Token)));
		const auto Archive = ReadValue<FSceneLightDiagnostic>(WriteValue(*Restored));
		HYP_CHECK(Archive.Type.WireName() == Name);
		const auto ExpectedKind = Name == "sky"           ? std::optional{ESceneLightDiagnosticKind::Sky}
		                          : Name == "directional" ? std::optional{ESceneLightDiagnosticKind::Directional}
		                                                  : std::nullopt;
		HYP_CHECK(Restored->Type.Kind() == ExpectedKind && Archive.Type.Kind() == ExpectedKind);
		const auto Legacy =
		    ParseJson("{\"type\":\"scene.light.diagnostic\",\"version\":1,\"fields\":{\"type\":" + Token + "}}");
		const auto LegacyRestored = std::static_pointer_cast<FSceneLightDiagnostic>(ReadRecord(Type, Legacy));
		HYP_CHECK(LegacyRestored->Type.WireName() == Name && LegacyRestored->Type.Kind() == ExpectedKind);
	}
	const auto Default = std::static_pointer_cast<FSceneLightDiagnostic>(ReadRecordWire(Type, ParseJson("{}")));
	HYP_CHECK(Default->Type.WireName().empty() && !Default->Type.Kind());
	for (const char* Invalid : {R"({"type":5})", R"({"type":null})"})
	{
		bool bRejected{};
		try
		{
			ReadRecordWire(Type, ParseJson(Invalid));
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
	}
	const auto Schema = RecordWireSchema(Type);
	const auto& Properties =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Schema.Value).at("properties").Value);
	HYP_CHECK(WriteJson(Properties.at("type")) == R"({"default":"","type":"string"})");
	HYP_CHECK(ResolveRecordMember(Type, &FSceneLightDiagnostic::Type).FieldId == "type");
}

void CheckNativeDiagnosticKinds()
{
	FSceneLightDiagnostic Entry;
	Entry.Type = FSceneLightDiagnosticType(ESceneLightDiagnosticKind::Directional);
	HYP_CHECK(Entry.Type.Kind() == ESceneLightDiagnosticKind::Directional && Entry.Type.WireName() == "directional");
	Entry.Type = FSceneLightDiagnosticType::FromWire("future");
	HYP_CHECK(!Entry.Type.Kind() && Entry.Type.WireName() == "future");
	Entry.Type = FSceneLightDiagnosticType(ESceneLightDiagnosticKind::Sky);
	HYP_CHECK(Entry.Type.Kind() == ESceneLightDiagnosticKind::Sky && Entry.Type.WireName() == "sky");
	const auto Restored = ReadValue<FSceneLightDiagnostic>(WriteValue(Entry));
	HYP_CHECK(Restored.Type.Kind() == ESceneLightDiagnosticKind::Sky);
	bool bRejected{};
	try
	{
		static_cast<void>(FSceneLightDiagnosticType(static_cast<ESceneLightDiagnosticKind>(999)));
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
} // namespace

int main()
{
	try
	{
		CheckWire();
		CheckSchemaAndDefaults();
		CheckDiagnosticWire();
		CheckNativeDiagnosticKinds();
		std::cout << "PASS: sky string wire/schema fixtures and typed failures\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
