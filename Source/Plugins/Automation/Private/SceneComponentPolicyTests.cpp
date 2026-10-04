#include "Hyperion/SceneEditing/SceneComponentEditPolicy.h"
#include "Hyperion/SceneEditing/SceneComponentEditing.h"
#include "SceneTestTarget.h"
#include <source_location>

namespace
{
using namespace Hyperion;

void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Scene policy check failed at " + std::to_string(InLocation.line()));
	}
}

template<class T> void CheckReadOnly(const FRecordDescriptor& InType, const T& InBefore, const T& InAfter)
{
	bool bRejected{};
	try
	{
		ValidateSceneComponentEdit(InType, &InBefore, &InAfter);
	}
	catch (const FSceneEditError& Error)
	{
		bRejected = Error.Code == SceneEditErrors::ReadOnly;
	}
	Check(bRejected);
}

void CheckTypedPolicy()
{
	auto Type = RecordType<FSceneModelComponent>();
	for (auto& Member : Type.Members)
	{
		Member.Options.Inspector.reset();
		// The rule follows C++ identity even when a descriptor's external names differ.
		Member.Id = "renamed-" + Member.Id;
	}
	FSceneModelComponent Before;
	Before.Asset = "/Game/model.asset";
	ValidateSceneComponentEdit(Type, &Before, &Before);
	for (const auto Field :
	     {&FSceneModelComponent::Asset, &FSceneModelComponent::SourceNode, &FSceneModelComponent::SourcePrimitive})
	{
		auto After = Before;
		After.*Field = "changed";
		CheckReadOnly(Type, Before, After);
		Check(IsSceneComponentFieldReadOnly(Type, ResolveRecordMember(Type, Field).FieldId));
	}
	auto After = Before;
	After.bVisible = false;
	ValidateSceneComponentEdit(Type, &Before, &After);
	Check(!IsSceneComponentFieldReadOnly(Type, "renamed-visible"));
	const FSceneModelSource Source{"asset", "node", "root"};
	for (const auto Field :
	     {&FSceneModelSource::Asset, &FSceneModelSource::SourceNode, &FSceneModelSource::InstanceRoot})
	{
		auto Candidate = Source;
		Candidate.*Field = "changed";
		CheckReadOnly(RecordType<FSceneModelSource>(), Source, Candidate);
	}
}

void CheckDraftAdmission()
{
	auto Type = RecordType<FSceneModelComponent>();
	for (auto& Member : Type.Members)
	{
		Member.Options.Inspector->bReadOnly = false;
	}
	FSceneModelComponent First;
	First.Asset = "first";
	FSceneModelComponent Second;
	Second.Asset = "second";
	FRecordDraft Single(Type, &First);
	Single.GetValues().at("asset") = WriteValue(std::string("replacement"));
	auto Candidate = First;
	Single.ApplyToCandidate(&Candidate);
	CheckReadOnly(Type, First, Candidate);
	const std::array<const void*, 2> Values{&First, &Second};
	FRecordSelectionDraft Selection(Type, Values);
	Selection.SetValue({"asset"}, WriteValue(std::string("replacement")));
	for (std::size_t Index = 0; Index < Values.size(); ++Index)
	{
		const auto& Original = *static_cast<const FSceneModelComponent*>(Values[Index]);
		Candidate = Original;
		Selection.ApplyToCandidate(Index, &Candidate);
		CheckReadOnly(Type, Original, Candidate);
	}
	FRecordSelectionDraft Valid(Type, Values);
	Valid.SetValue({"visible"}, WriteValue(false));
	for (std::size_t Index = 0; Index < Values.size(); ++Index)
	{
		const auto& Original = *static_cast<const FSceneModelComponent*>(Values[Index]);
		Candidate = Original;
		Valid.ApplyToCandidate(Index, &Candidate);
		ValidateSceneComponentEdit(Type, &Original, &Candidate);
		Check(Candidate.Asset == Original.Asset && !Candidate.bVisible);
	}
}

void CheckDocumentAdmission()
{
	FTaskSystem Tasks(1, 1);
	FSceneTestTarget Target(Tasks);
	FSceneNode First;
	First.Id = "policy-first";
	First.Model().emplace();
	First.Model()->Asset = "first";
	auto Second = First;
	Second.Id = "policy-second";
	Second.Model()->Asset = "second";
	const std::vector<FSceneHandle> Handles{Target.AddNode(First), Target.AddNode(Second)};
	FSceneEditDocument Document;
	Document.Attach(Target);
	const auto Revision = Target.Revision();
	const auto Component = RecordType<FSceneModelComponent>().Id;
	TSceneComponentBatchRequest<FSceneModelComponent> Request{
	    Document.Id(), Revision, Handles, {Component, Component}, {*First.Model(), *Second.Model()}};
	Request.Values[0].bVisible = false;
	Request.Values[1].Asset = "replacement";
	bool bRejected{};
	try
	{
		SetSceneComponentBatch(Document, Request);
	}
	catch (const FSceneEditError& Error)
	{
		bRejected = Error.Code == SceneEditErrors::ReadOnly;
	}
	Check(bRejected && Target.Revision() == Revision && !Document.IsDirty());
	Check(Document.GetState().History.empty() && Target.FindNode(Handles[0])->Model()->bVisible);
	Request.Values[1].Asset = "second";
	Request.Values[1].bVisible = false;
	SetSceneComponentBatch(Document, Request);
	Check(Document.GetState().HistoryCursor == 1 && Document.IsDirty());
	Check(Target.FindNode(Handles[0])->Model()->Asset == "first");
	Check(Target.FindNode(Handles[1])->Model()->Asset == "second");
	Check(!Target.FindNode(Handles[0])->Model()->bVisible && !Target.FindNode(Handles[1])->Model()->bVisible);
	Document.Undo();
	Check(Target.FindNode(Handles[0])->Model()->bVisible && Target.FindNode(Handles[1])->Model()->bVisible);
	Document.Redo();
	Check(!Target.FindNode(Handles[0])->Model()->bVisible && !Target.FindNode(Handles[1])->Model()->bVisible);
	Document.Detach(Tasks);
}
} // namespace

void CheckSceneComponentEditPolicy()
{
	CheckTypedPolicy();
	CheckDraftAdmission();
	CheckDocumentAdmission();
}
