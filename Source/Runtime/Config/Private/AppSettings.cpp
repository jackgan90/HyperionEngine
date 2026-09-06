#include <Hyperion/Config/AppSettings.h>
#include <type_traits>

namespace Hyperion
{
namespace
{
template<class M>
FProperty Field(std::string InId, std::string InLabel, M FAppSettings::* InMember, double InMinimum = 0,
                double InMaximum = 0)
{
	FProperty P;
	P.Id = std::move(InId);
	P.Label = std::move(InLabel);
	P.Minimum = InMinimum;
	P.Maximum = InMaximum;
	if constexpr (std::is_same_v<M, bool>)
	{
		P.Kind = EPropertyKind::Boolean;
	}
	else if constexpr (std::is_integral_v<M>)
	{
		P.Kind = EPropertyKind::Integer;
	}
	else if constexpr (std::is_floating_point_v<M>)
	{
		P.Kind = EPropertyKind::Number;
	}
	else if constexpr (std::is_same_v<M, std::string>)
	{
		P.Kind = EPropertyKind::String;
	}
	else
	{
		P.Kind = EPropertyKind::StringList;
	}
	P.Get = [InMember](const void* InObject) -> FValue
	{
		const auto& Value = static_cast<const FAppSettings*>(InObject)->*InMember;
		if constexpr (std::is_integral_v<M> && !std::is_same_v<M, bool>)
		{
			return static_cast<std::int64_t>(Value);
		}
		else
		{
			return Value;
		}
	};
	P.Set = [InMember](void* InObject, const FValue& InValue)
	{
		if constexpr (std::is_integral_v<M> && !std::is_same_v<M, bool>)
		{
			static_cast<FAppSettings*>(InObject)->*InMember = static_cast<M>(std::get<std::int64_t>(InValue));
		}
		else
		{
			static_cast<FAppSettings*>(InObject)->*InMember = std::get<M>(InValue);
		}
	};
	return P;
}
} // namespace

const FTypeDescriptor& SettingsType()
{
	static const FTypeDescriptor Type{
	    "hyperion.application-settings",
	    1,
	    {Field("title", "Window title", &FAppSettings::Title),
	     Field("width", "Window width", &FAppSettings::Width, 64, 8192),
	     Field("height", "Window height", &FAppSettings::Height, 64, 8192),
	     Field("workers", "CPU workers", &FAppSettings::Workers, 1, 64),
	     Field("rhi_threads", "RHI threads", &FAppSettings::RhiThreads, 1, 8),
	     Field("rhi_backend", "RHI backend (restart)", &FAppSettings::RHIBackend),
	     Field("model_source", "Model source (restart)", &FAppSettings::ModelSource),
	     Field("vsync", "Vertical sync", &FAppSettings::bVsync),
	     Field("show_gui", "Show debug UI", &FAppSettings::bShowGui),
	     Field("renderdoc_library", "RenderDoc DLL path (restart)", &FAppSettings::RenderDocLibrary),
	     Field("renderdoc_output", "RDC output directory (restart)", &FAppSettings::RenderDocOutput),
	     Field("renderdoc_auto_open", "Open RDC automatically", &FAppSettings::bRenderDocAutoOpen),
	     Field("triangle_scale", "Triangle scale", &FAppSettings::TriangleScale, 0.05, 1.5),
	     Field("clear_red", "Background red", &FAppSettings::ClearRed, 0, 1),
	     Field("clear_green", "Background green", &FAppSettings::ClearGreen, 0, 1),
	     Field("clear_blue", "Background blue", &FAppSettings::ClearBlue, 0, 1),
	     Field("plugins", "Enabled plugins (restart)", &FAppSettings::Plugins)}};
	return Type;
}

void SaveSettings(const std::filesystem::path& InPath, const FAppSettings& InSettings)
{
	SaveReflected(InPath, SettingsType(), &InSettings);
}

FAppSettings LoadSettings(const std::filesystem::path& InPath)
{
	FAppSettings Value;
	LoadReflected(InPath, SettingsType(), &Value);
	return Value;
}
} // namespace Hyperion
