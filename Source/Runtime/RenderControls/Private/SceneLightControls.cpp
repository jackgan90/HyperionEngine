#include "Hyperion/RenderControls/SceneLightControls.h"
#include <array>

namespace Hyperion
{
namespace
{
struct FLightKindName
{
	ESceneLightDiagnosticKind Kind;
	std::string_view Name;
};

constexpr std::array LightKinds{FLightKindName{ESceneLightDiagnosticKind::Directional, "directional"},
                                FLightKindName{ESceneLightDiagnosticKind::Sky, "sky"}};

std::string_view LightKindName(ESceneLightDiagnosticKind InKind)
{
	for (const auto& Entry : LightKinds)
	{
		if (Entry.Kind == InKind)
		{
			return Entry.Name;
		}
	}
	throw std::invalid_argument("Invalid light diagnostic kind");
}

FRecordMember LightTypeMember()
{
	FRecordMember Result;
	Result.Id = "type";
	Result.Shape = &RecordValueShape<std::string>;
	Result.Association = FRecordMemberAssociation(&FSceneLightDiagnostic::Type);
	Result.Write = [](const void* InObject)
	{
		return WriteValue(std::string(static_cast<const FSceneLightDiagnostic*>(InObject)->Type.WireName()));
	};
	Result.Read = [](void* InObject, const FArchiveNode& InNode, const FRecordReadContext& InContext)
	{
		static_cast<FSceneLightDiagnostic*>(InObject)->Type =
		    FSceneLightDiagnosticType::FromWire(ReadValue<std::string>(InNode, InContext));
	};
	Result.Visit = [](const void*, const FRecordVisitor&, std::string_view)
	{
	};
	return Result;
}

constexpr std::array SkyStates{std::pair{ESceneSkyState::None, std::string_view{}},
                               std::pair{ESceneSkyState::Unrequested, std::string_view{"unrequested"}},
                               std::pair{ESceneSkyState::Loading, std::string_view{"loading"}},
                               std::pair{ESceneSkyState::Uploading, std::string_view{"uploading"}},
                               std::pair{ESceneSkyState::Ready, std::string_view{"ready"}},
                               std::pair{ESceneSkyState::Failed, std::string_view{"failed"}}};

FRecordMember SkyStateMember()
{
	FRecordMember Result;
	Result.Id = "state";
	// The domain enum deliberately retains the established string wire/schema representation.
	Result.Shape = &RecordValueShape<std::string>;
	Result.Write = [](const void* InObject)
	{
		return WriteValue(std::string(SceneSkyStateName(static_cast<const FSceneSkyStatus*>(InObject)->State)));
	};
	Result.Read = [](void* InObject, const FArchiveNode& InNode, const FRecordReadContext& InContext)
	{
		const auto Name = ReadValue<std::string>(InNode, InContext);
		for (const auto& [State, Wire] : SkyStates)
		{
			if (Wire == Name)
			{
				static_cast<FSceneSkyStatus*>(InObject)->State = State;
				return;
			}
		}
		throw std::invalid_argument("Unknown sky preparation state: " + Name);
	};
	Result.Visit = [](const void*, const FRecordVisitor&, std::string_view)
	{
	};
	return Result;
}
} // namespace

FSceneLightDiagnosticType::FSceneLightDiagnosticType(ESceneLightDiagnosticKind InKind) : Value(InKind)
{
	LightKindName(InKind);
}

FSceneLightDiagnosticType FSceneLightDiagnosticType::FromWire(std::string InName)
{
	for (const auto& Entry : LightKinds)
	{
		if (Entry.Name == InName)
		{
			return FSceneLightDiagnosticType(Entry.Kind);
		}
	}
	FSceneLightDiagnosticType Result;
	Result.Value = std::move(InName);
	return Result;
}

std::optional<ESceneLightDiagnosticKind> FSceneLightDiagnosticType::Kind() const
{
	if (const auto* Kind = std::get_if<ESceneLightDiagnosticKind>(&Value))
	{
		return *Kind;
	}
	return {};
}

std::string_view FSceneLightDiagnosticType::WireName() const
{
	if (const auto* Name = std::get_if<std::string>(&Value))
	{
		return *Name;
	}
	return LightKindName(std::get<ESceneLightDiagnosticKind>(Value));
}

template<> std::span<const ESceneSkyState> RecordEnumValues<ESceneSkyState>()
{
	static const auto Values = []
	{
		std::array<ESceneSkyState, SkyStates.size()> Result;
		for (std::size_t Index = 0; Index < SkyStates.size(); ++Index)
		{
			Result[Index] = SkyStates[Index].first;
		}
		return Result;
	}();
	return Values;
}

std::string_view SceneSkyStateName(ESceneSkyState InState)
{
	for (const auto& [State, Wire] : SkyStates)
	{
		if (State == InState)
		{
			return Wire;
		}
	}
	throw std::invalid_argument("Invalid sky preparation state");
}

std::string FormatSceneSkyStatus(const FSceneSkyStatus& InStatus)
{
	switch (InStatus.State)
	{
		case ESceneSkyState::Failed:
			return "Failed: " + InStatus.Error;
		case ESceneSkyState::Ready:
			return "Ready";
		case ESceneSkyState::Uploading:
			return "Uploading";
		case ESceneSkyState::Loading:
			return "Loading";
		default:
			return "No sky requested";
	}
}

template<> const FRecordDescriptor& RecordType<FSceneSkyStatus>()
{
	static const auto Type =
	    MakeRecord<FSceneSkyStatus>("scene.sky.status", {SkyStateMember(), Member("error", &FSceneSkyStatus::Error)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneLightDiagnostic>()
{
	static const auto Type = MakeRecord<FSceneLightDiagnostic>(
	    "scene.light.diagnostic",
	    {Member("handle", &FSceneLightDiagnostic::Handle), Member("id", &FSceneLightDiagnostic::Id), LightTypeMember(),
	     Member("priority", &FSceneLightDiagnostic::Priority), Member("enabled", &FSceneLightDiagnostic::bEnabled),
	     Member("selected", &FSceneLightDiagnostic::bSelected), Member("tied", &FSceneLightDiagnostic::bTied),
	     Member("message", &FSceneLightDiagnostic::Message), Member("asset", &FSceneLightDiagnostic::Asset)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneLightingInfo>()
{
	static const auto Type = MakeRecord<FSceneLightingInfo>(
	    "scene.lighting.info",
	    {Member("revision", &FSceneLightingInfo::Revision),
	     Member("shadowDirectionalLight", &FSceneLightingInfo::ShadowDirectionalLight),
	     Member("skyLight", &FSceneLightingInfo::SkyLight), Member("lights", &FSceneLightingInfo::Lights)});
	return Type;
}
} // namespace Hyperion
