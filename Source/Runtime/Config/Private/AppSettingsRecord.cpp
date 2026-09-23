#include "Hyperion/Config/AppSettings.h"

namespace Hyperion
{
namespace
{
void ValidateSettings(const FAppSettings& InSettings)
{
	FAppSettings Checked;
	DecodeReflected(EncodeReflected(SettingsType(), &InSettings), SettingsType(), &Checked);
	if ((InSettings.RenderPipeline != "forward" && InSettings.RenderPipeline != "deferred") ||
	    (InSettings.GBufferLayout != "compact" && InSettings.GBufferLayout != "high"))
	{
		throw std::invalid_argument("Render pipeline must be forward/deferred and GBuffer layout compact/high");
	}
}

template<class T> FRecordMember SettingsMember(const FProperty& InProperty)
{
	FRecordMember Result;
	Result.Id = InProperty.Id;
	Result.Options.Description = InProperty.Label;
	Result.Options.bRequired = true;
	Result.Options.bPersistent = InProperty.bPersistent;
	Result.Shape = &RecordValueShape<T>;
	Result.Write = [Property = &InProperty](const void* InObject)
	{
		if constexpr (std::is_same_v<T, std::int32_t>)
		{
			return WriteValue(static_cast<T>(std::get<std::int64_t>(Property->Get(InObject))));
		}
		else
		{
			return WriteValue(std::get<T>(Property->Get(InObject)));
		}
	};
	Result.Read =
	    [Property = &InProperty](void* InObject, const FArchiveNode& InNode, const FRecordReadContext& InContext)
	{
		if constexpr (std::is_same_v<T, std::int32_t>)
		{
			Property->Set(InObject, std::int64_t(ReadValue<T>(InNode, InContext)));
		}
		else
		{
			Property->Set(InObject, ReadValue<T>(InNode, InContext));
		}
	};
	Result.Visit = [](const void*, const FRecordVisitor&, std::string_view)
	{
	};
	if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
	{
		Result.Options.Inspector =
		    FPropertyPresentation{InProperty.Label, {}, false, InProperty.Minimum, InProperty.Maximum};
	}
	return Result;
}
} // namespace

template<> const FRecordDescriptor& RecordType<FAppSettings>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FAppSettings>("hyperion.applicationsettings.values", {}, 1, ValidateSettings);
		// Reuse the existing GUI/persistence property inventory; new settings require one registration only.
		for (const auto& Property : SettingsType().Properties)
		{
			switch (Property.Kind)
			{
				case EPropertyKind::Boolean:
					Result.Members.push_back(SettingsMember<bool>(Property));
					break;
				case EPropertyKind::Integer:
					Result.Members.push_back(SettingsMember<std::int32_t>(Property));
					break;
				case EPropertyKind::Number:
					Result.Members.push_back(SettingsMember<double>(Property));
					break;
				case EPropertyKind::String:
					Result.Members.push_back(SettingsMember<std::string>(Property));
					break;
				case EPropertyKind::StringList:
					Result.Members.push_back(SettingsMember<std::vector<std::string>>(Property));
					break;
			}
		}
		return Result;
	}();
	return Type;
}

void ApplyAppSettings(FAppSettings& InTarget, const FAppSettings& InCandidate)
{
	ValidateSettings(InCandidate);
	InTarget = InCandidate;
}

bool EqualAppSettings(const FAppSettings& InFirst, const FAppSettings& InSecond)
{
	for (const auto& Property : SettingsType().Properties)
	{
		if (Property.Get(&InFirst) != Property.Get(&InSecond))
		{
			return false;
		}
	}
	return true;
}
} // namespace Hyperion
