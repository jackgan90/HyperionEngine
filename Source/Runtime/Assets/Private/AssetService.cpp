#include "Hyperion/Assets/AssetService.h"
#include <algorithm>
#include <cctype>

namespace Hyperion
{
namespace
{
std::string Extension(const std::filesystem::path& InPath)
{
	auto Value = InPath.extension().string();
	std::transform(Value.begin(), Value.end(), Value.begin(),
	               [](unsigned char InCharacter)
	               {
		               return static_cast<char>(std::tolower(InCharacter));
	               });
	return Value;
}
} // namespace

FAssetService::~FAssetService()
{
	Cancellation.Cancel();
	Drain();
}

void FAssetService::Register(FAssetCodec InCodec)
{
	std::lock_guard Lock(Mutex);
	if (Loading || Closing || !InCodec.Load || InCodec.TypeId.empty())
	{
		throw std::logic_error("Register codecs before loading");
	}
	for (const auto& Codec : Codecs)
	{
		for (const auto& Ext : InCodec.Extensions)
		{
			if (Codec.TypeId == InCodec.TypeId &&
			    std::find(Codec.Extensions.begin(), Codec.Extensions.end(), Ext) != Codec.Extensions.end())
			{
				throw std::logic_error("Duplicate asset codec");
			}
		}
	}
	Codecs.push_back(std::move(InCodec));
}

TAsyncResult<std::shared_ptr<const void>> FAssetService::Load(const std::filesystem::path& InPath,
                                                              const FRecordDescriptor& InType)
{
	const auto Path = std::filesystem::absolute(InPath).lexically_normal();
	const auto Key = std::make_pair(Path, InType.Id);
	std::lock_guard Lock(Mutex);
	if (Closing)
	{
		throw std::logic_error("Asset service is closing");
	}
	Loading = true;
	if (auto It = Cache.find(Key); It != Cache.end())
	{
		return It->second;
	}
	const auto Ext = Extension(Path);
	std::function<std::shared_ptr<void>(FAssetLoadContext&)> Import;
	for (const auto& Codec : Codecs)
	{
		if (Codec.TypeId == InType.Id &&
		    std::find(Codec.Extensions.begin(), Codec.Extensions.end(), Ext) != Codec.Extensions.end())
		{
			Import = Codec.Load;
			break;
		}
	}
	auto Result = DispatchAsync<std::shared_ptr<const void>>(
	    IO.TaskSystem(), {EDomain::Worker},
	    [this, Path, Ext, Type = InType, Import]
	    {
		    auto Bytes = IO.ReadAsync(Path, Cancellation).Get(IO.TaskSystem());
		    FAssetLoadContext Context{IO, Path, Bytes, Cancellation};
		    if (Ext == ".hasset")
		    {
			    return std::shared_ptr<const void>(ReadRecord(Type, DecodeArchive(*Bytes)));
		    }
		    if (!Import)
		    {
			    throw std::runtime_error("No asset reader for " + Ext + " and " + Type.Id);
		    }
		    return std::shared_ptr<const void>(Import(Context));
	    },
	    Cancellation);
	Cache.emplace(Key, Result);
	Pending.push_back(Result.Task());
	return Result;
}

TAsyncResult<bool> FAssetService::Save(std::filesystem::path InPath, const FRecordDescriptor& InType,
                                       std::shared_ptr<const void> InSnapshot)
{
	std::lock_guard Lock(Mutex);
	if (Closing || !InSnapshot)
	{
		throw std::logic_error("Invalid asset save request");
	}
	auto Result = DispatchAsync<bool>(
	    IO.TaskSystem(), {EDomain::Worker},
	    [this, Path = std::filesystem::absolute(InPath).lexically_normal(), Type = InType,
	     Snapshot = std::move(InSnapshot)]
	    {
		    if (Extension(Path) != ".hasset")
		    {
			    throw std::runtime_error("Asset export is supported only for .hasset archives");
		    }
		    auto Bytes = EncodeArchive(WriteRecord(Type, Snapshot.get()));
		    const bool Written = *IO.WriteAsync(Path, std::move(Bytes), Cancellation).Get(IO.TaskSystem());
		    std::lock_guard CacheLock(Mutex);
		    Cache.erase(std::make_pair(Path, Type.Id));
		    return Written;
	    },
	    Cancellation);
	Pending.push_back(Result.Task());
	return Result;
}

void FAssetService::Drain()
{
	std::vector<FTaskHandle> Work;
	{
		std::lock_guard Lock(Mutex);
		Closing = true;
		Work = Pending;
	}
	for (const auto& Task : Work)
	{
		try
		{
			IO.TaskSystem().Wait(Task);
		}
		catch (...)
		{ /* Requests retain their errors. */
		}
	}
}

void FAssetService::ClearCache()
{
	std::lock_guard Lock(Mutex);
	Cache.clear();
}
} // namespace Hyperion
