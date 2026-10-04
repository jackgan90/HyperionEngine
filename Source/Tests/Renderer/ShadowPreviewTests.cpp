#include "Hyperion/Config/AppSettings.h"
#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/Scene/LightShadows.h"
#include "Support/TestSupport.h"
#include <array>
#include <limits>

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
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

template<class T> void CheckAuthoring(std::uint32_t InMode, std::string_view InTypeId)
{
	const auto& Type = RecordType<T>();
	HYP_CHECK(Type.Id == InTypeId && Type.Version == 1);
	T Settings;
	Settings.DebugMode = static_cast<decltype(Settings.DebugMode)>(InMode);
	HYP_CHECK(ReadValue<T>(WriteValue(Settings)) == Settings);
	const auto Wire = WriteRecordWire(Type, &Settings);
	HYP_CHECK(ReadValue<std::uint32_t>(std::get<FArchiveNode::FObject>(Wire.Value).at("debugMode")) == InMode);
	const auto Restored = std::static_pointer_cast<T>(ReadRecordWire(Type, Wire));
	HYP_CHECK(*Restored == Settings);
	for (const auto& Member : Type.Members)
	{
		if (Member.Id == "debugMode")
		{
			HYP_CHECK(Member.Shape().Kind == ERecordValueKind::UnsignedInteger);
			HYP_CHECK(Member.Shape().ElementBytes == 4 && Member.Shape().EnumValues.empty());
			const auto& Choices = Member.Options.Inspector->Choices;
			const auto Index = PropertyChoiceIndex(Choices, WriteValue(InMode));
			HYP_CHECK(Index && ReadValue<std::uint32_t>(Choices[*Index].Value) == InMode);
		}
	}
}

void CheckDirectional()
{
	const std::array Modes{EDirectionalShadowPreview::Lit,           EDirectionalShadowPreview::CascadeColors,
	                       EDirectionalShadowPreview::Cascade0Depth, EDirectionalShadowPreview::Cascade1Depth,
	                       EDirectionalShadowPreview::Cascade2Depth, EDirectionalShadowPreview::Cascade3Depth};
	const std::array<std::string_view, 6> Labels{
	    "Lit", "Cascade colors", "Cascade 0 depth", "Cascade 1 depth", "Cascade 2 depth", "Cascade 3 depth"};
	for (std::uint32_t Value = 0; Value < Modes.size(); ++Value)
	{
		HYP_CHECK(ParseDirectionalShadowPreview(Value) == Modes[Value]);
		HYP_CHECK(ToShadowPreviewWireValue(Modes[Value]) == Value);
		HYP_CHECK(DescribeShadowPreview(Modes[Value]).Label == Labels[Value]);
		HYP_CHECK(ShadowPreviewCascade(Modes[Value]) == (Value < 2 ? std::nullopt : std::optional(Value - 2)));
		CheckAuthoring<FDirectionalShadowSettings>(Value, "hyperion.light.directional-shadows");
	}
	Rejects(
	    []
	    {
		    (void)ParseDirectionalShadowPreview(6);
	    });
	Rejects(
	    []
	    {
		    (void)ParseDirectionalShadowPreview(UINT32_MAX);
	    });
	Rejects(
	    []
	    {
		    (void)ToShadowPreviewWireValue(EDirectionalShadowPreview::Count);
	    });
}

void CheckContact()
{
	const std::array Modes{EContactShadowPreview::Lit, EContactShadowPreview::VisibilityMask,
	                       EContactShadowPreview::HierarchicalDepth};
	const std::array<std::string_view, 3> Labels{"Lit", "Visibility mask", "HZB depth"};
	for (std::uint32_t Value = 0; Value < Modes.size(); ++Value)
	{
		HYP_CHECK(ParseContactShadowPreview(Value) == Modes[Value]);
		HYP_CHECK(ParseAppContactShadowPreview(Value) == Modes[Value]);
		HYP_CHECK(ToShadowPreviewWireValue(Modes[Value]) == Value);
		HYP_CHECK(DescribeShadowPreview(Modes[Value]).Label == Labels[Value]);
		CheckAuthoring<FContactShadowSettings>(Value, "hyperion.contact.settings");
	}
	Rejects(
	    []
	    {
		    (void)ParseContactShadowPreview(3);
	    });
	Rejects(
	    []
	    {
		    (void)ParseContactShadowPreview(UINT32_MAX);
	    });
	Rejects(
	    []
	    {
		    (void)ToShadowPreviewWireValue(EContactShadowPreview::Count);
	    });
	for (const auto Value : {std::int64_t{-1}, std::numeric_limits<std::int64_t>::max()})
	{
		Rejects(
		    [&]
		    {
			    (void)ParseAppContactShadowPreview(Value);
		    });
	}
	HYP_CHECK(ContactShadowPreviewMinimum() == 0 && ContactShadowPreviewMaximum() == 2);
}
} // namespace

void CheckShadowPreviews()
{
	CheckDirectional();
	CheckContact();
}
