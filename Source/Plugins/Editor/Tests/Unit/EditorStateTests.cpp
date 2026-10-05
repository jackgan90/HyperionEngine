#include "../../Private/EditorInspectionCache.h"
#include "../../Private/EditorSelection.h"
#include "../../Private/OutlinerSelection.h"
#include "Hyperion/SceneEditing/SceneHistory.h"
#include <source_location>
#include <stdexcept>

using namespace Hyperion;

namespace
{
void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Editor state check failed at " + std::to_string(InLocation.line()));
	}
}

void CheckSelection()
{
	const FSceneHandle A{1, 3, 4};
	const FSceneHandle B{1, 3, 5};
	const FSceneHandle C{2, 3, 4};
	FEditorSelection Selection(A);
	Selection.Toggle(B);
	Selection.Toggle(C);
	Check(Selection.Contains(A) && Selection.Contains(B) && Selection.Contains(C));
	Check(!Selection.Contains({1, 3, 6}) && Selection.Primary() == C);
	Selection.Toggle(B);
	Check(!Selection.Contains(B) && Selection.Primary() == C);
	const auto Copy = Selection;
	Selection.Clear();
	Check(!Selection && Copy.Contains(A));
	Selection = Copy;
	Selection.Remap(FSceneHandleMap{{A, B}, {C, A}});
	Check(Selection.All() == std::vector<FSceneHandle>({B, A}));
	Check(Selection.Contains(A) && Selection.Contains(B) && !Selection.Contains(C));
	Selection = std::nullopt;
	Check(!Selection && !Selection.Contains(A));
}

void CheckOutlinerRanges()
{
	const FSceneHandle A{1, 0, 1};
	const FSceneHandle B{1, 1, 1};
	const FSceneHandle C{1, 2, 1};
	const FSceneHandle D{1, 3, 1};
	const std::array Rows{A, C, B, D};
	FOutlinerSelectionState State;
	auto Selected = State.Click({}, Rows, C, false, false);
	Selected = State.Click(Selected, Rows, D, true, false);
	Check(Selected.All() == std::vector<FSceneHandle>({C, B, D}));
	Selected = State.Click(Selected, Rows, A, true, false);
	Check(Selected.All() == std::vector<FSceneHandle>({C, A}) && Selected.Primary() == A);
	Selected = State.Click(Selected, Rows, D, true, true);
	Check(Selected.All() == std::vector<FSceneHandle>({C, A, B, D}));
	Selected = State.Click(Selected, Rows, B, true, true);
	Check(Selected.All().size() == 4 && Selected.Primary() == B);
	State.Reset();
	const std::array Filtered{D, A};
	Selected = State.Click(Selected, Filtered, D, true, false);
	Check(Selected.All() == std::vector<FSceneHandle>({D}));
	Selected = State.Click(Selected, Filtered, A, true, false);
	Check(Selected.All() == std::vector<FSceneHandle>({D, A}));
	// Slot reuse cannot retain the deleted anchor's generation.
	const FSceneHandle NewD{1, 3, 2};
	const std::array Restored{NewD, B, A};
	Selected = State.Click(Selected, Restored, B, true, false);
	Check(Selected.All() == std::vector<FSceneHandle>({A, B}));
	Selected = State.Click(Selected, Restored, B, false, true);
	Check(!Selected.Contains(B) && Selected.Contains(A));
	Selected = State.Click(Selected, Restored, NewD, true, false);
	Check(Selected.All() == std::vector<FSceneHandle>({B, NewD}));
	std::vector<FSceneHandle> Large;
	for (unsigned Index = 0; Index < 512; ++Index)
	{
		Large.push_back({2, Index, 1});
	}
	State.Reset();
	Selected = State.Click({}, Large, Large.front(), false, false);
	Selected = State.Click(Selected, Large, Large.back(), true, false);
	Check(Selected.All() == Large);
}

void CheckHistoryRemapping()
{
	const FSceneHandle A{1, 0, 1};
	const FSceneHandle B{1, 1, 1};
	const FSceneHandle NewA{1, 1, 2};
	const FSceneHandle NewB{1, 0, 2};
	std::vector<FSceneHistoryEntry> History(2);
	for (auto& Entry : History)
	{
		Entry.Handle = A;
		Entry.BeforeSettings.DefaultCamera = A;
		Entry.AfterSettings.DefaultCamera = B;
		Entry.DeletedSubtree = {{A, {}}, {B, {}}};
		Entry.Edits = {{B, {}, {}}};
		Entry.DeletedRoots = {A};
		Entry.BeforeSelection.Toggle(A);
		Entry.BeforeSelection.Toggle(B);
	}
	RemapSceneHistory(History, {{A, NewA}, {B, NewB}});
	for (const auto& Entry : History)
	{
		Check(Entry.Handle == NewA && Entry.BeforeSettings.DefaultCamera == NewA);
		Check(Entry.AfterSettings.DefaultCamera == NewB);
		Check(Entry.DeletedSubtree[0].first == NewA && Entry.DeletedSubtree[1].first == NewB);
		Check(Entry.Edits[0].Handle == NewB && Entry.DeletedRoots.front() == NewA);
		Check(Entry.BeforeSelection.All() == std::vector<FSceneHandle>({NewA, NewB}));
	}
	RemapSceneHistory(History, {{NewA, A}, {NewB, B}});
	Check(History.front().BeforeSelection.Primary() == B);
}

void CheckInspectionCache()
{
	FSceneNode A;
	FSceneNode B;
	A.Local() = Translation({1, 2, 3});
	B.Local() = Translation({7, 8, 9});
	const auto& Component = A.Components.All().front();
	Check(Component.Type->CppType == typeid(FSceneTransform));
	const auto& Other = B.Components.All().front();
	const std::array<const void*, 2> Values{Component.Get(), Other.Get()};
	const std::array<FSceneHandle, 2> Targets{{{1, 0, 1}, {1, 1, 1}}};
	FEditorInspectionCache Cache;
	Cache.Prepare(1, 1, Targets);
	FEditorInspectionCache::FSelectionEntry Entry;
	Entry.Type = Component.Type;
	Entry.Components = {Component.Id, Other.Id};
	Entry.Draft = std::make_unique<FRecordSelectionDraft>(*Component.Type->Record, Values);
	auto* Draft = Cache.StoreSelection(std::move(Entry)).Draft.get();
	const FInspectionPath X{"position", "fields", "x"};
	Check(Draft->IsMixed(X) && Draft->IsMixed(X));
	Cache.Prepare(1, 1, Targets);
	Check(Cache.FindSelection(*Component.Type)->Draft.get() == Draft);
	Draft->SetValue(X, WriteValue(4.f));
	Check(!Draft->IsMixed(X));
	auto Edited = Cache.TakeSelection(Component.Type->Id);
	Check(!Cache.FindSelection(*Component.Type));
	Edited->ApplyToCandidate(1, B.Components.Find(Other.Id)->Edit());
	Check(B.Local().Values[12] == 4 && B.Local().Values[13] == 8);
	// A rejected edit is taken out of the cache even when the scene revision does not advance.
	auto& Single = Cache.Single(Component);
	Single.GetValues().at("position") = WriteValue(FVec3{20, 30, 40});
	auto Rejected = Cache.TakeSingle(Component.Id);
	Check(ReadValue<FVec3>(Cache.Single(Component).GetValues().at("position")).X == 1);
	A.Local().Values[12] = 11;
	Cache.Prepare(1, 2, Targets);
	Check(ReadValue<FVec3>(Cache.Single(Component).GetValues().at("position")).X == 11);
	A.Local().Values[12] = 12;
	Cache.Prepare(2, 2, Targets);
	Check(ReadValue<FVec3>(Cache.Single(Component).GetValues().at("position")).X == 12);
	A.Local().Values[12] = 13;
	const std::array Reordered{Targets[1], Targets[0]};
	Cache.Prepare(2, 2, Reordered);
	Check(ReadValue<FVec3>(Cache.Single(Component).GetValues().at("position")).X == 13);
}
} // namespace

int main()
{
	CheckSelection();
	CheckOutlinerRanges();
	CheckHistoryRemapping();
	CheckInspectionCache();
}
