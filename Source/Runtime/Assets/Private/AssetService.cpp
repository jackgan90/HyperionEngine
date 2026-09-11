#include "AssetServiceInternal.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <cctype>

namespace Hyperion
{
std::filesystem::path NormalizeAssetPath(const std::filesystem::path& InPath)
{
	return std::filesystem::absolute(InPath).lexically_normal();
}

void RequireNativeAssetPath(const std::filesystem::path& InPath)
{
	auto Extension = InPath.extension().string();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(),
	               [](unsigned char InCharacter)
	               {
		               return static_cast<char>(std::tolower(InCharacter));
	               });
	if (Extension != ".hasset")
	{
		throw std::runtime_error("Runtime assets require .hasset; import the source with hyperion_asset_tool import");
	}
}

FAssetService::FAssetService(FIOService& InIO, FAssetCacheOptions InOptions)
    : Impl(std::make_unique<FImpl>(InIO, InOptions))
{
	if (!InOptions.MaxInFlight || !InOptions.MaxGraphAssets)
	{
		throw std::invalid_argument("Invalid asset service limits");
	}
}

FAssetService::~FAssetService()
{
	Impl->Cancellation.Cancel();
	Drain();
}

FRecordRegistry& FAssetService::Types()
{
	return Impl->Registry;
}

void FAssetService::FImpl::RequireOpen() const
{
	if (bClosing)
	{
		throw std::logic_error("Asset service is closing");
	}
}

FLoadedAsset FAssetService::FImpl::Read(const std::filesystem::path& InPath)
{
	try
	{
		RequireNativeAssetPath(InPath);
		auto Bytes = IO.ReadAsync(InPath, Cancellation).Get(IO.TaskSystem());
		HYP_PERF_SCOPE_C(Assets, LoadNativeAsset);
		Cancellation.Check();
		auto Document = DecodeAsset(Bytes);
		FLoadedAsset Result;
		Result.Type = Registry.Find(Document.Header.TypeId);
		Result.Path = InPath;
		Result.Header = std::move(Document.Header);
		Result.Object = ReadRecord(*Result.Type, Document.Object, {InPath.generic_string(), &Result.Diagnostics});
		Result.Header.Dependencies = CollectAssetDependencies(*Result.Type, Result.Object.get());
		Result.RetainedBytes = Document.StoredBytes * 2;
		if (Document.bLegacy)
		{
			Result.Diagnostics.push_back(
			    "Legacy archive loaded; use hyperion_asset_tool upgrade to publish a native asset");
		}
		Cancellation.Check();
		return Result;
	}
	catch (const std::exception& Error)
	{
		throw std::runtime_error(InPath.generic_string() + ": " + Error.what());
	}
}

FAssetRequest FAssetService::LoadAsync(const std::filesystem::path& InPath)
{
	return Load(InPath, false);
}

FAssetRequest FAssetService::Load(const std::filesystem::path& InPath, bool bInGraphDependency)
{
	auto& P = *Impl;
	const auto Path = NormalizeAssetPath(InPath);
	std::lock_guard Lock(P.Mutex);
	if (!bInGraphDependency)
	{
		P.RequireOpen();
	}
	const auto Stats = P.Trim();
	if (const auto It = P.Cache.find(Path); It != P.Cache.end())
	{
		if (!It->second.Request.Ready())
		{
			It->second.LastUse = ++P.Access;
			return FAssetRequest(It->second.Request);
		}
		try
		{
			(void)It->second.Request.GetReady();
			It->second.LastUse = ++P.Access;
			return FAssetRequest(It->second.Request);
		}
		catch (...)
		{
			P.Cache.erase(It);
		}
	}
	if (Stats.InFlight >= P.Options.MaxInFlight)
	{
		throw std::runtime_error("Asset in-flight request limit exceeded");
	}
	std::optional<FTaskHandle> Tail;
	if (const auto It = P.Operations.find(Path); It != P.Operations.end())
	{
		Tail = It->second;
	}
	std::optional<TAsyncResult<bool>> Barrier;
	if (const auto It = P.Writes.find(Path); It != P.Writes.end())
	{
		Barrier = It->second;
	}
	P.Pending.reserve(P.Pending.size() + 2);
	auto Request = DispatchAsync<FLoadedAsset>(
	    P.IO.TaskSystem(), {EDomain::Worker},
	    [State = Impl.get(), Path, Barrier, Tail]
	    {
		    if (Tail)
		    {
			    try
			    {
				    State->IO.TaskSystem().Wait(*Tail);
			    }
			    catch (...)
			    {
			    }
		    }
		    if (Barrier)
		    {
			    (void)Barrier->Get(State->IO.TaskSystem());
		    }
		    return State->Read(Path);
	    },
	    P.Cancellation);
	P.Pending.push_back(Request.Task());
	P.Operations[Path] = Request.Task();
	P.Cache.emplace(Path, FImpl::FCacheEntry{Request, ++P.Access});
	P.ScheduleTrim(Request.Task());
	return FAssetRequest(std::move(Request));
}

TAsyncResult<bool> FAssetService::Save(std::filesystem::path InPath, const FRecordDescriptor& InType,
                                       std::shared_ptr<const void> InSnapshot)
{
	auto& P = *Impl;
	const auto Path = NormalizeAssetPath(InPath);
	std::lock_guard Lock(P.Mutex);
	P.RequireOpen();
	P.Trim();
	std::optional<FTaskHandle> Tail;
	if (const auto It = P.Operations.find(Path); It != P.Operations.end())
	{
		Tail = It->second;
	}
	std::optional<TAsyncResult<bool>> Previous;
	if (const auto It = P.Writes.find(Path); It != P.Writes.end())
	{
		Previous = It->second;
	}
	P.Cache.erase(Path);
	if (P.Pending.size() >= P.Options.MaxInFlight)
	{
		throw std::runtime_error("Asset in-flight request limit exceeded");
	}
	P.Pending.reserve(P.Pending.size() + 2);
	auto Request = DispatchAsync<bool>(
	    P.IO.TaskSystem(), {EDomain::Worker},
	    [State = Impl.get(), Path, Type = InType, Snapshot = std::move(InSnapshot), Previous, Tail]
	    {
		    if (Tail)
		    {
			    try
			    {
				    State->IO.TaskSystem().Wait(*Tail);
			    }
			    catch (...)
			    {
			    }
		    }
		    RequireNativeAssetPath(Path);
		    if (Previous)
		    {
			    // A failed earlier save does not prevent a later corrective save.
			    try
			    {
				    (void)Previous->Get(State->IO.TaskSystem());
			    }
			    catch (...)
			    {
			    }
		    }
		    FAssetHeader Header;
		    const auto Existing = State->IO.TryReadAsync(Path, State->Cancellation).Get(State->IO.TaskSystem());
		    if (*Existing)
		    {
			    const auto Old = DecodeAsset(std::make_shared<const FBytes>(**Existing));
			    if (Old.Header.TypeId != Type.Id)
			    {
				    throw std::runtime_error("Cannot overwrite an asset with a different type");
			    }
			    Header.Id = Old.Header.Id;
		    }
		    auto Encoded = EncodeAsset(Type, Snapshot.get(), std::move(Header));
		    return *State->IO.WriteAsync(Path, std::move(Encoded.Bytes), State->Cancellation)
		                .Get(State->IO.TaskSystem());
	    },
	    P.Cancellation);
	P.Pending.push_back(Request.Task());
	P.Writes[Path] = Request;
	P.Operations[Path] = Request.Task();
	P.ScheduleTrim(Request.Task());
	return Request;
}

void FAssetService::Drain()
{
	auto& P = *Impl;
	{
		std::lock_guard Lock(P.Mutex);
		P.bClosing = true;
	}
	for (;;)
	{
		std::vector<FTaskHandle> Work;
		{
			std::lock_guard Lock(P.Mutex);
			P.Trim();
			if (P.Pending.empty())
			{
				break;
			}
			Work = P.Pending;
		}
		for (const auto& Task : Work)
		{
			try
			{
				P.IO.TaskSystem().Wait(Task);
			}
			catch (...)
			{
			}
		}
		// Accepted graph work may admit dependency producers and their cleanup while we wait.
	}
	{
		std::lock_guard Lock(P.Mutex);
		P.Pending.clear();
		P.Cache.clear();
		P.Writes.clear();
		P.Operations.clear();
	}
}

void FAssetRequest::Validate(const FLoadedAsset& InAsset) const
{
	if (Expected &&
	    (Expected->TypeId != InAsset.Header.TypeId || (!Expected->Id.empty() && Expected->Id != InAsset.Header.Id) ||
	     (!Expected->Revision.empty() && Expected->Revision != InAsset.Header.Revision)))
	{
		throw std::runtime_error("Asset reference identity/type/revision mismatch: " + InAsset.Path.generic_string());
	}
}

std::shared_ptr<const FLoadedAsset> FAssetRequest::GetReady() const
{
	Cancellation.Check();
	auto Asset = Result.GetReady();
	Validate(*Asset);
	return Asset;
}

std::shared_ptr<const FLoadedAsset> FAssetRequest::Get(FTaskSystem& InTasks) const
{
	Cancellation.Check();
	auto Asset = Result.Get(InTasks);
	Cancellation.Check();
	Validate(*Asset);
	return Asset;
}
} // namespace Hyperion
