#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

struct FAssetReferenceInput
{
	FGui Gui;
	FAssetRef Value{"11111111111111111111111111111111", "/Game/Old.hasset", "test.sky", std::string(64, 'a')};
	FAssetRef Candidate{"22222222222222222222222222222222", "/Game/New.hasset", "test.sky", {}};
	FVec4 Source;
	FVec4 Target;
	FGuiEditState Edit;
	unsigned Changes{};
	bool bDisabled{};
	bool bMixed{};
	bool bCompatible = true;

	FAssetReferenceInput()
	{
		Gui.FontImage();
		Gui.SetAssetReferenceProvider(
		    [&](std::string_view InType)
		    {
			    HYP_CHECK(InType == "test.sky");
			    return bCompatible ? std::vector{Candidate} : std::vector<FAssetRef>{};
		    });
		Frame();
		Frame();
	}

	void Frame(std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, InEvents);
		Gui.BeginPanel("Source", {0, 0}, {220, 400});
		Gui.Selectable("Asset", false);
		Source = Gui.LastItemBounds();
		Gui.DragSource(AssetPathPayloadType, Candidate.Path, "Asset");
		Gui.EndPanel();
		Gui.BeginPanel("Target", {250, 0}, {500, 500});
		Gui.BeginDisabled(bDisabled);
		Gui.BeginLiveEdit();
		Gui.BeginPropertyRow("Sky");
		Changes += Gui.EditAssetReference(Value, "test.sky", bMixed);
		Target = Gui.LastItemBounds();
		Gui.EndPropertyRow();
		Edit = Gui.EndLiveEdit();
		Gui.EndDisabled();
		Gui.EndPanel();
		Gui.Render();
	}

	void Move(float InX, float InY)
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseMove;
		Event.X = InX;
		Event.Y = InY;
		Frame(std::span(&Event, 1));
	}

	void Button(bool bInDown)
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseButton;
		Event.bDown = bInDown;
		Frame(std::span(&Event, 1));
	}

	void Drop()
	{
		Move((Source.X + Source.Z) / 2, (Source.Y + Source.W) / 2);
		Button(true);
		Move((Source.X + Source.Z) / 2 + 12, (Source.Y + Source.W) / 2);
		HYP_CHECK(Gui.DragPayload());
		Move((Target.X + Target.Z) / 2, (Target.Y + Target.W) / 2);
		Frame();
		Button(false);
	}
};

void CheckReferenceDrop(bool bInSameId, bool bInDisabled, bool bInCompatible, bool bInMixed)
{
	FAssetReferenceInput Input;
	Input.bDisabled = bInDisabled;
	Input.bCompatible = bInCompatible;
	Input.bMixed = bInMixed;
	if (bInSameId)
	{
		Input.Candidate.Id = Input.Value.Id;
	}
	if (bInMixed)
	{
		// A mixed selection still submits when the primary already equals the chosen reference.
		Input.Value = Input.Candidate;
	}
	const auto Before = Input.Value;
	Input.Drop();
	const bool bExpected = !bInDisabled && bInCompatible;
	HYP_CHECK(Input.Value == (bExpected ? Input.Candidate : Before));
	HYP_CHECK(Input.Changes == unsigned(bExpected));
	HYP_CHECK((Input.Edit.ChangedInteraction != 0) == bExpected);
	Input.Frame();
	HYP_CHECK(Input.Changes == unsigned(bExpected));
}
} // namespace

void CheckAssetReferenceControls()
{
	CheckReferenceDrop(false, false, true, false);
	CheckReferenceDrop(true, false, true, false);
	CheckReferenceDrop(false, true, true, false);
	CheckReferenceDrop(false, false, false, false);
	CheckReferenceDrop(true, false, true, true);
}
