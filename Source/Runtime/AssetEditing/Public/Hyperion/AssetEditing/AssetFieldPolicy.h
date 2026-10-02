#pragma once
#include "Hyperion/Reflection/Record.h"

namespace Hyperion
{
enum class EAssetFieldRoute
{
	ReadOnly,
	Field,
	ModelPrimitives,
	TextureEncoding
};

class FAssetFieldPolicy
{
public:
	const FRecordMemberIdentity& Field() const
	{
		return Identity;
	}

	EAssetFieldRoute Route() const
	{
		return EditRoute;
	}

private:
	friend FAssetFieldPolicy ResolveAssetFieldPolicy(const FRecordDescriptor&, const FRecordMemberIdentity&);

	FAssetFieldPolicy(FRecordMemberIdentity InIdentity, EAssetFieldRoute InRoute)
	    : Identity(std::move(InIdentity)), EditRoute(InRoute)
	{
	}

	FRecordMemberIdentity Identity;
	EAssetFieldRoute EditRoute{};
};

FAssetFieldPolicy ResolveAssetFieldPolicy(const FRecordDescriptor& InType, const FRecordMemberIdentity& InField);
FAssetFieldPolicy ResolveAssetFieldPolicy(const FRecordDescriptor& InType, std::string_view InField);
FAssetFieldPolicy AssetNamePolicy(const FRecordDescriptor& InType);

template<class T, class M> FAssetFieldPolicy ResolveAssetFieldPolicy(const FRecordDescriptor& InType, M T::* InMember)
{
	return ResolveAssetFieldPolicy(InType, ResolveRecordMember(InType, InMember));
}
} // namespace Hyperion
