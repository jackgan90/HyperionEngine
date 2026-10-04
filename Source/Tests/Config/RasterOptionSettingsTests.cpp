#include "Hyperion/Config/AppSettings.h"
#include "Hyperion/Reflection/Wire.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <fstream>

namespace
{
using namespace Hyperion;

template<class TAction> void Rejects(TAction InAction)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

std::string ReadBytes(const char* InPath)
{
	std::ifstream File(InPath, std::ios::binary);
	HYP_CHECK(File.good());
	return {std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
}

void CheckConfigReadRejection(const char* InField, const FValue& InInvalid)
{
	const auto& Type = SettingsType();
	FAppSettings Settings;
	Settings.Title = "Original title";
	Settings.Width = 1440;
	Settings.Plugins = {"graphics", "gui"};
	const auto Before = Settings;
	auto Document = ParseJson(EncodeReflected(Type, &Settings));
	auto& Properties =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Document.Value).at("properties").Value);
	Properties["title"] = WriteValue(std::string("Changed before invalid option"));
	Properties[InField] = std::visit(
	    [](const auto& InValue)
	    {
		    return WriteValue(InValue);
	    },
	    InInvalid);
	const auto Text = WriteJson(Document);
	Rejects(
	    [&]
	    {
		    DecodeReflected(Text, Type, &Settings);
	    });
	HYP_CHECK(EqualAppSettings(Settings, Before));
	std::ofstream("invalid-raster-options.json", std::ios::binary) << Text;
	Rejects(
	    [&]
	    {
		    LoadReflected("invalid-raster-options.json", Type, &Settings);
	    });
	HYP_CHECK(EqualAppSettings(Settings, Before));
	Rejects(
	    [&]
	    {
		    (void)LoadSettings("invalid-raster-options.json");
	    });
	const auto Property = std::find_if(Type.Properties.begin(), Type.Properties.end(),
	                                   [&](const auto& InProperty)
	                                   {
		                                   return InProperty.Id == InField;
	                                   });
	HYP_CHECK(Property != Type.Properties.end() && Property->Validate);
	Rejects(
	    [&]
	    {
		    Property->Set(&Settings, InInvalid);
	    });
	HYP_CHECK(EqualAppSettings(Settings, Before));

	auto Record = WriteRecord(RecordType<FAppSettings>(), &Settings);
	std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Record.Value).at("fields").Value)[InField] =
	    Properties[InField];
	Rejects(
	    [&]
	    {
		    (void)ReadRecord(RecordType<FAppSettings>(), Record);
	    });
	auto Wire = WriteRecordWire(RecordType<FAppSettings>(), &Settings);
	std::get<FArchiveNode::FObject>(Wire.Value)[InField] = Properties[InField];
	Rejects(
	    [&]
	    {
		    (void)ReadRecordWire(RecordType<FAppSettings>(), Wire);
	    });
}

void CheckConfigSaveRejection(const char* InField, const FValue& InInvalid)
{
	const auto& Type = SettingsType();
	FAppSettings Settings;
	SaveSettings("preserved-raster-options.json", Settings);
	const auto Bytes = ReadBytes("preserved-raster-options.json");
	if (std::string_view(InField) == "render_pipeline")
	{
		Settings.RenderPipeline = std::get<std::string>(InInvalid);
	}
	else if (std::string_view(InField) == "gbuffer_layout")
	{
		Settings.GBufferLayout = std::get<std::string>(InInvalid);
	}
	else if (std::string_view(InField) == "contact_shadow_debug")
	{
		Settings.ContactShadowDebug = static_cast<int>(std::get<std::int64_t>(InInvalid));
	}
	else
	{
		Settings.GBufferDebug = static_cast<int>(std::get<std::int64_t>(InInvalid));
	}
	Rejects(
	    [&]
	    {
		    (void)EncodeReflected(Type, &Settings);
	    });
	Rejects(
	    [&]
	    {
		    (void)WriteRecord(RecordType<FAppSettings>(), &Settings);
	    });
	Rejects(
	    [&]
	    {
		    (void)WriteRecordWire(RecordType<FAppSettings>(), &Settings);
	    });
	Rejects(
	    [&]
	    {
		    SaveReflected("preserved-raster-options.json", Type, &Settings);
	    });
	HYP_CHECK(ReadBytes("preserved-raster-options.json") == Bytes);
	Rejects(
	    [&]
	    {
		    SaveSettings("preserved-raster-options.json", Settings);
	    });
	HYP_CHECK(ReadBytes("preserved-raster-options.json") == Bytes);
}

void CheckConfigRejection(const char* InField, const FValue& InInvalid)
{
	CheckConfigReadRejection(InField, InInvalid);
	CheckConfigSaveRejection(InField, InInvalid);
}

void CheckPurePreflight()
{
	struct FProbe
	{
		std::string First = "original";
		std::string Second = "valid";
	} Value;

	unsigned Setters{};
	unsigned Validations{};
	const auto Property = [&](const char* InId, std::string FProbe::* InMember)
	{
		FProperty Result;
		Result.Id = InId;
		Result.Kind = EPropertyKind::String;
		Result.Get = [InMember](const void* InObject) -> FValue
		{
			return static_cast<const FProbe*>(InObject)->*InMember;
		};
		Result.Set = [&, InMember](void* InObject, const FValue& InValue)
		{
			++Setters;
			static_cast<FProbe*>(InObject)->*InMember = std::get<std::string>(InValue);
		};
		return Result;
	};
	FTypeDescriptor Type{"pure-preflight", 1, {Property("first", &FProbe::First), Property("second", &FProbe::Second)}};
	Type.Properties[1].Validate = [&](const FValue& InValue)
	{
		HYP_CHECK(Setters == 0);
		++Validations;
		if (std::get<std::string>(InValue) == "denied")
		{
			throw std::invalid_argument("Denied by pure value validator");
		}
	};
	const auto Invalid =
	    R"({"type":"pure-preflight","schema_version":1,"properties":{"first":"changed","second":"denied"}})";
	Rejects(
	    [&]
	    {
		    DecodeReflected(Invalid, Type, &Value);
	    });
	HYP_CHECK(Setters == 0 && Validations == 1 && Value.First == "original" && Value.Second == "valid");
	DecodeReflected(R"({"type":"pure-preflight","schema_version":1,"properties":{"first":"changed","second":"valid"}})",
	                Type, &Value);
	HYP_CHECK(Setters == 2 && Validations == 2 && Value.First == "changed");
	Setters = 0;
	Value.Second = "denied";
	Rejects(
	    [&]
	    {
		    (void)EncodeReflected(Type, &Value);
	    });
	HYP_CHECK(Setters == 0 && Validations == 3);
	Type.Properties[1].Validate = {};
	DecodeReflected(Invalid, Type, &Value);
	HYP_CHECK(Setters == 2 && Validations == 3 && Value.Second == "denied");
	HYP_CHECK(!EncodeReflected(Type, &Value).empty());
}
} // namespace

void CheckRasterOptionSettings()
{
	CheckConfigRejection("render_pipeline", std::string("unknown"));
	CheckConfigRejection("gbuffer_layout", std::string("unknown"));
	CheckConfigRejection("gbuffer_debug", std::int64_t(-1));
	CheckConfigRejection("gbuffer_debug", std::int64_t(7));
	CheckConfigRejection("contact_shadow_debug", std::int64_t(-1));
	CheckConfigRejection("contact_shadow_debug", std::int64_t(3));
	CheckPurePreflight();
}
