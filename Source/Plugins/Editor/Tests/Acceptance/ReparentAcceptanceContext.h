#pragma once
#include "Hyperion/Math/Math.h"
#include "Hyperion/Scene/Scene.h"
#include <algorithm>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Hyperion
{
enum class EReparentFixtureRole
{
	PrimaryModel,
	SecondaryModel,
	PrimaryChild,
	ParentTarget,
	SingularTarget,
	PointLight,
};

struct FReparentFixture
{
	EReparentFixtureRole Role;
	FSceneHandle Handle;
	std::string Id;
	FMat4 InitialWorld;
	std::string BeforeParent;
	std::string ExpectedParent;
};

struct FReparentAcceptanceContext
{
	std::size_t CaseIndex{};
	std::vector<FReparentFixture> Fixtures;

	FReparentFixture& Fixture(EReparentFixtureRole InRole)
	{
		return const_cast<FReparentFixture&>(std::as_const(*this).Fixture(InRole));
	}

	const FReparentFixture& Fixture(EReparentFixtureRole InRole) const
	{
		const auto Found = std::find_if(Fixtures.begin(), Fixtures.end(),
		                                [InRole](const FReparentFixture& InFixture)
		                                {
			                                return InFixture.Role == InRole;
		                                });
		if (Found == Fixtures.end())
		{
			throw std::logic_error("Missing Reparent acceptance fixture");
		}
		return *Found;
	}
};

enum class EReparentCase
{
	GroupToParent,
	FilteredGroupToRoot,
	RootNoOp,
	RejectCycle,
	RejectSingularParent,
	CancelEscape,
	CancelFocusLoss,
	DropOutside,
	CancelRevisionChange,
	FilteredSingleToParent,
	MixedToParent,
	CancelRightButton,
	CancelDocumentInvalidation,
	CancelSelectionChange,
};

enum class EReparentInterruption
{
	None,
	Escape,
	FocusLoss,
	RevisionChange,
	RightButton,
	DocumentInvalidation,
	SelectionChange,
};

enum class EReparentOutcome
{
	Changed,
	NoOp,
	Rejected,
	Cancelled,
};

struct FSceneRootTarget
{
};

struct FOutsideTarget
{
};

struct FReparentCaseDefinition
{
	EReparentCase Id;
	std::string_view Name;
	std::span<const EReparentFixtureRole> Selection;
	std::span<const EReparentFixtureRole> MovedRoots;
	EReparentFixtureRole Source;
	std::variant<FSceneRootTarget, EReparentFixtureRole, FOutsideTarget> Target;
	bool bFiltered;
	EReparentInterruption Interruption;
	EReparentOutcome Outcome;
};

inline constexpr EReparentFixtureRole GroupSelection[]{
    EReparentFixtureRole::PrimaryModel, EReparentFixtureRole::PrimaryChild, EReparentFixtureRole::SecondaryModel};
inline constexpr EReparentFixtureRole GroupRoots[]{EReparentFixtureRole::PrimaryModel,
                                                   EReparentFixtureRole::SecondaryModel};
inline constexpr EReparentFixtureRole SingleSelection[]{EReparentFixtureRole::SecondaryModel};
inline constexpr EReparentFixtureRole MixedSelection[]{EReparentFixtureRole::PointLight,
                                                       EReparentFixtureRole::SecondaryModel};

inline constexpr FReparentCaseDefinition ReparentCases[]{
    {EReparentCase::GroupToParent, "group to parent", GroupSelection, GroupRoots, EReparentFixtureRole::PrimaryModel,
     EReparentFixtureRole::ParentTarget, false, EReparentInterruption::None, EReparentOutcome::Changed},
    {EReparentCase::FilteredGroupToRoot, "filtered group to root", GroupSelection, GroupRoots,
     EReparentFixtureRole::PrimaryModel, FSceneRootTarget{}, true, EReparentInterruption::None,
     EReparentOutcome::Changed},
    {EReparentCase::RootNoOp, "root no-op", GroupSelection, GroupRoots, EReparentFixtureRole::PrimaryModel,
     FSceneRootTarget{}, false, EReparentInterruption::None, EReparentOutcome::NoOp},
    {EReparentCase::RejectCycle, "reject cycle", GroupSelection, GroupRoots, EReparentFixtureRole::PrimaryModel,
     EReparentFixtureRole::PrimaryChild, false, EReparentInterruption::None, EReparentOutcome::Rejected},
    {EReparentCase::RejectSingularParent, "reject singular parent", GroupSelection, GroupRoots,
     EReparentFixtureRole::PrimaryModel, EReparentFixtureRole::SingularTarget, false, EReparentInterruption::None,
     EReparentOutcome::Rejected},
    {EReparentCase::CancelEscape, "cancel Escape", GroupSelection, GroupRoots, EReparentFixtureRole::PrimaryModel,
     EReparentFixtureRole::ParentTarget, false, EReparentInterruption::Escape, EReparentOutcome::Cancelled},
    {EReparentCase::CancelFocusLoss, "cancel focus loss", GroupSelection, GroupRoots,
     EReparentFixtureRole::PrimaryModel, EReparentFixtureRole::ParentTarget, false, EReparentInterruption::FocusLoss,
     EReparentOutcome::Cancelled},
    {EReparentCase::DropOutside, "drop outside", GroupSelection, GroupRoots, EReparentFixtureRole::PrimaryModel,
     FOutsideTarget{}, false, EReparentInterruption::None, EReparentOutcome::Cancelled},
    {EReparentCase::CancelRevisionChange, "cancel revision change", GroupSelection, GroupRoots,
     EReparentFixtureRole::PrimaryModel, EReparentFixtureRole::ParentTarget, false,
     EReparentInterruption::RevisionChange, EReparentOutcome::Cancelled},
    {EReparentCase::FilteredSingleToParent, "filtered single to parent", SingleSelection, SingleSelection,
     EReparentFixtureRole::SecondaryModel, EReparentFixtureRole::ParentTarget, true, EReparentInterruption::None,
     EReparentOutcome::Changed},
    {EReparentCase::MixedToParent, "mixed light/model to parent", MixedSelection, MixedSelection,
     EReparentFixtureRole::PointLight, EReparentFixtureRole::ParentTarget, false, EReparentInterruption::None,
     EReparentOutcome::Changed},
    {EReparentCase::CancelRightButton, "cancel right button", GroupSelection, GroupRoots,
     EReparentFixtureRole::PrimaryModel, EReparentFixtureRole::ParentTarget, false, EReparentInterruption::RightButton,
     EReparentOutcome::Cancelled},
    {EReparentCase::CancelDocumentInvalidation, "cancel document invalidation", GroupSelection, GroupRoots,
     EReparentFixtureRole::PrimaryModel, EReparentFixtureRole::ParentTarget, false,
     EReparentInterruption::DocumentInvalidation, EReparentOutcome::Cancelled},
    {EReparentCase::CancelSelectionChange, "cancel selection change", GroupSelection, GroupRoots,
     EReparentFixtureRole::PrimaryModel, EReparentFixtureRole::ParentTarget, false,
     EReparentInterruption::SelectionChange, EReparentOutcome::Cancelled},
};
} // namespace Hyperion
