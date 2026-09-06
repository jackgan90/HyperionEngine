#include <Hyperion/Reflection/Reflection.h>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace Hyperion
{
namespace
{
using FJson = nlohmann::json;

FValue ReadValue(const FJson& InJson, const FProperty& InProperty)
{
	auto Bad = [&]
	{
		return std::runtime_error("Invalid value for property '" + InProperty.Id + "'");
	};
	switch (InProperty.Kind)
	{
		case EPropertyKind::Boolean:
			if (!InJson.is_boolean())
			{
				throw Bad();
			}
			return InJson.get<bool>();
		case EPropertyKind::Integer:
		{
			if (!InJson.is_number_integer())
			{
				throw Bad();
			}
			const auto Value = InJson.get<double>();
			if (!std::isfinite(Value) || Value < InProperty.Minimum || Value > InProperty.Maximum)
			{
				throw Bad();
			}
			return InJson.get<std::int64_t>();
		}
		case EPropertyKind::Number:
		{
			if (!InJson.is_number())
			{
				throw Bad();
			}
			const auto Value = InJson.get<double>();
			if (!std::isfinite(Value) || Value < InProperty.Minimum || Value > InProperty.Maximum)
			{
				throw Bad();
			}
			return Value;
		}
		case EPropertyKind::String:
			if (!InJson.is_string())
			{
				throw Bad();
			}
			return InJson.get<std::string>();
		case EPropertyKind::StringList:
			if (!InJson.is_array())
			{
				throw Bad();
			}
			for (const auto& Item : InJson)
			{
				if (!Item.is_string())
				{
					throw Bad();
				}
			}
			return InJson.get<std::vector<std::string>>();
	}
	throw Bad();
}
} // namespace

std::string EncodeReflected(const FTypeDescriptor& InType, const void* InObject)
{
	FJson Document{{"type", InType.Id}, {"schema_version", InType.Version}, {"properties", FJson::object()}};
	for (const auto& Property : InType.Properties)
	{
		if (Property.bPersistent)
		{
			FJson Value = std::visit(
			    [](const auto& InItem) -> FJson
			    {
				    return InItem;
			    },
			    Property.Get(InObject));
			(void)ReadValue(Value, Property);
			Document["properties"][Property.Id] = std::move(Value);
		}
	}
	return Document.dump(2) + '\n';
}

void SaveReflected(const std::filesystem::path& InPath, const FTypeDescriptor& InType, const void* InObject)
{
	const auto Text = EncodeReflected(InType, InObject);
	if (!InPath.parent_path().empty())
	{
		std::filesystem::create_directories(InPath.parent_path());
	}
	auto Temporary = InPath;
	Temporary += ".tmp";
	{
		std::ofstream File(Temporary, std::ios::binary | std::ios::trunc);
		File << Text;
		File.flush();
		if (!File)
		{
			throw std::runtime_error("Cannot write configuration: " + Temporary.string());
		}
	}
#ifdef _WIN32
	if (!MoveFileExW(Temporary.c_str(), InPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		throw std::runtime_error("Cannot replace configuration: " + InPath.string());
	}
#else
	std::filesystem::rename(Temporary, InPath);
#endif
}

void LoadReflected(const std::filesystem::path& InPath, const FTypeDescriptor& InType, void* InObject)
{
	std::ifstream File(InPath, std::ios::binary);
	if (!File)
	{
		throw std::runtime_error("Cannot read configuration: " + InPath.string());
	}
	const std::string Text{std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
	DecodeReflected(Text, InType, InObject);
}

void DecodeReflected(std::string_view InText, const FTypeDescriptor& InType, void* InObject)
{
	const FJson Document = FJson::parse(InText);
	if (!Document.is_object() || Document.at("type") != InType.Id)
	{
		throw std::runtime_error("Configuration type mismatch");
	}
	const auto& Version = Document.at("schema_version");
	if (!Version.is_number_integer() || Version.get<double>() < 1 || Version.get<double>() > InType.Version)
	{
		throw std::runtime_error("Unsupported configuration schema version");
	}
	const auto& Properties = Document.at("properties");
	if (!Properties.is_object())
	{
		throw std::runtime_error("Expected configuration properties object");
	}
	std::vector<std::pair<const FProperty*, FValue>> Pending;
	for (const auto& Property : InType.Properties)
	{
		if (Property.bPersistent && Properties.contains(Property.Id))
		{
			Pending.emplace_back(&Property, ReadValue(Properties.at(Property.Id), Property));
		}
	}
	for (const auto& [property, value] : Pending)
	{
		property->Set(InObject, value);
	}
}
} // namespace Hyperion
