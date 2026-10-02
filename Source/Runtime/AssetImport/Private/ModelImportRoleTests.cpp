#include "Hyperion/Assets/NativeAsset.h"
#include "ModelImportInternal.h"
#include <functional>

namespace Hyperion::Private
{
namespace
{
void Require(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

void CheckSameOutput(const FModelSourceAssets& InExpected, const FModelSourceAssets& InActual)
{
	Require(Serialize(InExpected.Model) == Serialize(InActual.Model), "Role order changed model content");
	Require(InExpected.Products.size() == InActual.Products.size(), "Role order changed product count");
	FAssetHeader Header;
	Header.Id = "0123456789abcdef0123456789abcdef";
	for (std::size_t Index = 0; Index < InExpected.Products.size(); ++Index)
	{
		const auto& Expected = InExpected.Products[Index];
		const auto& Actual = InActual.Products[Index];
		Require(Expected.Key == Actual.Key && Expected.SharedKey == Actual.SharedKey &&
		            Expected.Type->Id == Actual.Type->Id,
		        "Role order changed product or sharing identity");
		const auto Before = EncodeAsset(*Expected.Type, Expected.Object.get(), Header);
		const auto After = EncodeAsset(*Actual.Type, Actual.Object.get(), Header);
		Require(Before.Bytes == After.Bytes && Before.Header.Revision == After.Header.Revision,
		        "Role order changed native product bytes or revision");
	}
}

void CheckRejectedRoles(const FModelSource& InSource, FModelTextureRoles InRoles)
{
	bool bRejected{};
	try
	{
		(void)SplitModelSourceWithRoles(InSource, std::move(InRoles));
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	Require(bRejected, "Invalid model role descriptions were accepted");
}

void CheckInvalidRoles(const FModelSource& InSource)
{
	const std::array<std::function<void(FModelTextureRoles&)>, 8> Mutations{
	    [](auto& OutRoles)
	    {
		    OutRoles[0].OutputOrder = OutRoles.size();
	    },
	    [](auto& OutRoles)
	    {
		    OutRoles[0].OutputOrder = OutRoles[1].OutputOrder;
	    },
	    [](auto& OutRoles)
	    {
		    OutRoles[0].SourceMember = nullptr;
	    },
	    [](auto& OutRoles)
	    {
		    OutRoles[0].SourceMember = OutRoles[1].SourceMember;
	    },
	    [](auto& OutRoles)
	    {
		    OutRoles[0].TextureSemantic = OutRoles[1].TextureSemantic;
	    },
	    [](auto& OutRoles)
	    {
		    OutRoles[0].SamplerSemantic = OutRoles[1].SamplerSemantic;
	    },
	    [](auto& OutRoles)
	    {
		    OutRoles[0].UvSemantic = OutRoles[1].UvSemantic;
	    },
	    [](auto& OutRoles)
	    {
		    OutRoles[0].Encoding = static_cast<EMaterialTextureEncoding>(255);
	    }};
	for (const auto& Mutate : Mutations)
	{
		auto Roles = ModelTextureRoles;
		Mutate(Roles);
		CheckRejectedRoles(InSource, std::move(Roles));
	}
}
} // namespace

void CheckModelTextureRoleDescriptions(const FModelSource& InSource)
{
	const auto Expected = SplitModelSource(InSource);
	auto Roles = OrderedModelTextureRoles;
	std::size_t Count{};
	do
	{
		CheckSameOutput(Expected, SplitModelSourceWithRoles(InSource, Roles));
		++Count;
	} while (std::next_permutation(Roles.begin(), Roles.end(),
	                               [](const auto& InLeft, const auto& InRight)
	                               {
		                               return InLeft.OutputOrder < InRight.OutputOrder;
	                               }));
	Require(Count == 120, "Not all model role permutations were checked");
	CheckInvalidRoles(InSource);
}
} // namespace Hyperion::Private
