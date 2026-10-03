#include "Hyperion/SceneEditing/SceneComponentEditPolicy.h"
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
namespace
{
bool IsReadOnly(const FRecordDescriptor& InType, const FRecordMember& InMember)
{
	if (InType.CppType == typeid(FSceneModelSource))
	{
		return true;
	}
	return InType.CppType == typeid(FSceneModelComponent) &&
	       (InMember.Association.Matches(&FSceneModelComponent::Asset) ||
	        InMember.Association.Matches(&FSceneModelComponent::SourceNode) ||
	        InMember.Association.Matches(&FSceneModelComponent::SourcePrimitive));
}
} // namespace

bool IsSceneComponentFieldReadOnly(const FRecordDescriptor& InType, std::string_view InField)
{
	for (const auto& Member : InType.Members)
	{
		if (Member.Id == InField)
		{
			return IsReadOnly(InType, Member);
		}
	}
	return false;
}

void ValidateSceneComponentEdit(const FRecordDescriptor& InType, const void* InOriginal, const void* InCandidate)
{
	for (const auto& Member : InType.Members)
	{
		if (Member.Options.bPersistent && IsReadOnly(InType, Member) &&
		    !EqualInspectionValue(Member.Write(InOriginal), Member.Write(InCandidate)))
		{
			throw FSceneEditError("read_only", "Immutable component field: " + Member.Id);
		}
	}
}
} // namespace Hyperion
