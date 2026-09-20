#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "SceneInstanceInternal.h"

namespace Hyperion
{
std::string FSceneInstance::RegisterModelAsset(FAssetRef InReference, const std::filesystem::path& InContainingAsset)
{
	auto& P = *Impl;
	P.RequireOpen();
	if (!P.Status.bLoaded || !P.Status.Error.empty() || InReference.TypeId != RecordType<FModelAsset>().Id)
	{
		throw std::invalid_argument("Model registration requires a loaded scene and a native model reference");
	}
	const auto Containing = InContainingAsset.empty() ? P.Path : InContainingAsset;
	InReference.Path = PathToUtf8(P.Assets.Resolve(InReference, Containing));
	for (const auto& [Id, Load] : P.Loads)
	{
		auto Reference = Load.Reference;
		try
		{
			Reference.Path = PathToUtf8(P.Assets.Resolve(Reference, P.Path));
		}
		catch (const std::exception&)
		{
			// An old unresolved reference retains its own load error without blocking unrelated registrations.
			continue;
		}
		if (Reference == InReference)
		{
			return Id;
		}
	}
	std::size_t Index = P.Loads.size();
	std::string Id;
	do
	{
		Id = "registered-model-" + std::to_string(++Index);
	} while (P.Loads.contains(Id));
	auto& Load = P.Loads[Id];
	Load.Reference = std::move(InReference);
	Load.Epoch = P.LoadEpoch;
	try
	{
		Load.Preparation = LoadNativeModel(P.Assets, P.Tasks, Load.Reference, {}, Load.Cancellation,
		                                   &P.Session.GetResources(), P.bPrepareQueries);
	}
	catch (const std::exception& Failure)
	{
		Load.Error = Failure.what();
		Load.bComplete = true;
	}
	return Id;
}
} // namespace Hyperion
