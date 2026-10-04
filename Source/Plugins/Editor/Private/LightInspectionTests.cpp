#include "LightInspection.h"
#include <iostream>
#include <source_location>

using namespace Hyperion;

namespace
{
void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Light inspection check failed at line " + std::to_string(InLocation.line()));
	}
}

void CheckMemberRouting()
{
	for (const auto* Type : {&RecordType<FSceneDirectionalLight>(), &RecordType<FSceneEnvironmentLight>()})
	{
		const bool bSky = Type->CppType == typeid(FSceneEnvironmentLight);
		const auto ExpectedKind = bSky ? ESceneLightDiagnosticKind::Sky : ESceneLightDiagnosticKind::Directional;
		auto Renamed = *Type;
		for (auto& Member : Renamed.Members)
		{
			const auto Before = FindLightDiagnosticProperty(*Type, Member.Id);
			Member.Id = "renamed-" + Member.Id;
			if (Member.Options.Inspector)
			{
				Member.Options.Inspector->Label = "Unrelated label";
			}
			const auto After = FindLightDiagnosticProperty(Renamed, Member.Id);
			Check(Before.has_value() == After.has_value());
			if (Before)
			{
				Check(After->Kind == ExpectedKind && After->Property == Before->Property);
			}
		}
		Check(!FindLightDiagnosticProperty(Renamed, "priority"));
		Check(!FindLightDiagnosticProperty(Renamed, "sky"));
		Check(!FindLightDiagnosticProperty(Renamed, "missing"));
		Renamed.Members = {Member("priority", &FSceneDirectionalLight::Intensity),
		                   Member("sky", &FSceneEnvironmentLight::Intensity)};
		Check(!FindLightDiagnosticProperty(Renamed, "priority"));
		Check(!FindLightDiagnosticProperty(Renamed, "sky"));
		Renamed.Members = Type->Members;
		Renamed.CppType = typeid(FScenePointLight);
		Check(!FindLightDiagnosticProperty(Renamed, "priority"));
	}
	const auto Directional = FindLightDiagnosticProperty(RecordType<FSceneDirectionalLight>(), "priority");
	const auto Sky = FindLightDiagnosticProperty(RecordType<FSceneEnvironmentLight>(), "sky");
	Check(Directional && Directional->Property == ELightDiagnosticProperty::Priority);
	Check(Sky && Sky->Property == ELightDiagnosticProperty::SkyAsset);
}

FPropertyPresentation BasePresentation()
{
	FPropertyPresentation Result;
	Result.Label = "Existing label";
	Result.Tooltip = "Existing tooltip";
	Result.TooltipLines.push_back({"Existing line"});
	return Result;
}

void CheckPriorityPresentation(ESceneLightDiagnosticKind InKind)
{
	auto Node = MakeSceneDirectionalLightNode("sun");
	const FSceneNodeView View{{1, 2, 3}, &Node};
	FSceneLightingInfo Info;
	Info.Lights.emplace_back();
	auto& Entry = Info.Lights.back();
	Entry.Handle = View.Handle;
	Entry.Type = FSceneLightDiagnosticType(InKind);
	Entry.Message = "The authoritative diagnostic message";
	const FLightDiagnosticProperty Property{InKind, ELightDiagnosticProperty::Priority};
	const bool bSky = InKind == ESceneLightDiagnosticKind::Sky;
	for (const bool bEnabled : {false, true})
	{
		for (const bool bSelected : {false, true})
		{
			for (const bool bTied : {false, true})
			{
				Entry.bEnabled = bEnabled;
				Entry.bSelected = bSelected;
				Entry.bTied = bTied;
				auto Actual = BasePresentation();
				auto Expected = Actual;
				Expected.TooltipLines.push_back({Entry.Message, !bEnabled   ? EPropertyTooltipTone::Default
				                                                : bSelected ? EPropertyTooltipTone::Positive
				                                                            : EPropertyTooltipTone::Negative});
				if (bEnabled && !bSelected)
				{
					if (!bSky)
					{
						Expected.TooltipLines.push_back({"This light still contributes direct lighting."});
					}
					Expected.TooltipLines.push_back(
					    {bSky ? "Increase Priority above the other sky lights to make this sky light effective."
					          : "Increase Priority above the other shadow-casting directional lights to make this "
					            "light the shadow source."});
				}
				if (bTied)
				{
					Expected.WarningTooltip =
					    bSky ? "Multiple enabled Sky Lights share the highest Priority."
					         : "Multiple Directional Lights eligible to cast shadows share the highest Priority.";
				}
				InspectLightDiagnostic(View, Property, Info, Actual);
				Check(Actual == Expected);
			}
		}
	}
}

void CheckExcludedAndUnrelated()
{
	auto Node = MakeSceneDirectionalLightNode("sun");
	Node.DirectionalLight()->Intensity = 0;
	const FSceneNodeView View{{1, 2, 3}, &Node};
	FSceneLightingInfo Info;
	Info.Lights.emplace_back();
	auto& Entry = Info.Lights.back();
	Entry.Handle = View.Handle;
	Entry.Type = FSceneLightDiagnosticType(ESceneLightDiagnosticKind::Directional);
	Entry.bEnabled = true;
	Entry.Message = "Excluded";
	const FLightDiagnosticProperty Property{ESceneLightDiagnosticKind::Directional, ELightDiagnosticProperty::Priority};
	auto Actual = BasePresentation();
	auto Expected = Actual;
	Expected.TooltipLines.push_back({"Excluded", EPropertyTooltipTone::Default});
	InspectLightDiagnostic(View, Property, Info, Actual);
	Check(Actual == Expected);
	for (const char* Token : {"sky", "future", "", "Directional"})
	{
		Entry.Type = FSceneLightDiagnosticType::FromWire(Token);
		Actual = BasePresentation();
		InspectLightDiagnostic(View, Property, Info, Actual);
		Check(Actual == BasePresentation());
	}
	Entry.Type = FSceneLightDiagnosticType(ESceneLightDiagnosticKind::Directional);
	++Entry.Handle.Generation;
	Actual = BasePresentation();
	InspectLightDiagnostic(View, Property, Info, Actual);
	Check(Actual == BasePresentation());
}

void CheckSkyPresentation()
{
	const FSceneNodeView View{{1, 2, 3}};
	FSceneLightingInfo Info;
	Info.Lights.emplace_back();
	auto& Entry = Info.Lights.back();
	Entry.Handle = View.Handle;
	Entry.Type = FSceneLightDiagnosticType(ESceneLightDiagnosticKind::Sky);
	const FLightDiagnosticProperty Property{ESceneLightDiagnosticKind::Sky, ELightDiagnosticProperty::SkyAsset};
	const std::array Cases{std::pair{ESceneSkyState::Unrequested, "Sky asset: unrequested"},
	                       std::pair{ESceneSkyState::Loading, "Sky asset: loading"},
	                       std::pair{ESceneSkyState::Uploading, "Sky asset: uploading"}};
	for (const auto& [State, Tooltip] : Cases)
	{
		Entry.Asset = {State, "Error text does not determine state"};
		auto Actual = BasePresentation();
		auto Expected = Actual;
		Expected.Label += " [...]";
		Expected.Tooltip = Tooltip;
		InspectLightDiagnostic(View, Property, Info, Actual);
		Check(Actual == Expected);
	}
	for (const char* Error : {"", "Preparing", "Failed: details"})
	{
		Entry.Asset = {ESceneSkyState::Failed, Error};
		auto Actual = BasePresentation();
		auto Expected = Actual;
		Expected.Label += " [!]";
		Expected.Tooltip = Error;
		InspectLightDiagnostic(View, Property, Info, Actual);
		Check(Actual == Expected);
	}
	for (const auto State : {ESceneSkyState::None, ESceneSkyState::Ready})
	{
		Entry.Asset = {State, "Failed: irrelevant"};
		auto Actual = BasePresentation();
		InspectLightDiagnostic(View, Property, Info, Actual);
		Check(Actual == BasePresentation());
	}
}
} // namespace

int main()
{
	try
	{
		CheckMemberRouting();
		CheckPriorityPresentation(ESceneLightDiagnosticKind::Directional);
		CheckPriorityPresentation(ESceneLightDiagnosticKind::Sky);
		CheckExcludedAndUnrelated();
		CheckSkyPresentation();
		std::cout << "PASS: typed light Inspector routing and presentation\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
