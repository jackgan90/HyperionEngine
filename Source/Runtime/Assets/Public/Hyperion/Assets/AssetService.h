#pragma once
#include "Hyperion/IO/IOService.h"
#include "Hyperion/Serialization/Archive.h"

namespace Hyperion
{
struct FAssetLoadContext
{
	FIOService& IO;
	std::filesystem::path Path;
	std::shared_ptr<const FBytes> Bytes;
	FCancellationToken Cancellation;

	std::shared_ptr<const FBytes> Read(const std::filesystem::path& InPath) const
	{
		Cancellation.Check();
		return IO.ReadAsync(InPath, Cancellation).Get(IO.TaskSystem());
	}
};

struct FAssetCodec
{
	std::string TypeId;
	std::vector<std::string> Extensions;
	std::function<std::shared_ptr<void>(FAssetLoadContext&)> Load;
};

template<class T> class TAssetRequest
{
public:
	TAssetRequest() = default;

	explicit TAssetRequest(TAsyncResult<std::shared_ptr<const void>> InResult) : Result(std::move(InResult))
	{
	}

	void Cancel() const
	{
		Cancellation.Cancel();
	}

	bool Ready() const
	{
		return Cancellation.IsCancelled() || Result.Ready();
	}

	std::shared_ptr<const T> GetReady() const
	{
		Cancellation.Check();
		return std::static_pointer_cast<const T>(*Result.GetReady());
	}

	std::shared_ptr<const T> Get(FTaskSystem& InTasks) const
	{
		Cancellation.Check();
		auto Value = Result.Get(InTasks);
		Cancellation.Check();
		return std::static_pointer_cast<const T>(*Value);
	}

private:
	TAsyncResult<std::shared_ptr<const void>> Result;
	FCancellationToken Cancellation;
};

// Own before requests and destroy/drain before IO and Tasks. Register codecs
// before loading. Results are immutable CPU assets; GPU readiness is separate.
class FAssetService
{
public:
	explicit FAssetService(FIOService& InIO) : IO(InIO)
	{
	}

	~FAssetService();
	void Register(FAssetCodec InCodec);

	template<class T> TAssetRequest<T> LoadAsync(const std::filesystem::path& InPath)
	{
		return TAssetRequest<T>(Load(InPath, RecordType<T>()));
	}

	template<class T> TAsyncResult<bool> SaveAsync(std::filesystem::path InPath, std::shared_ptr<const T> InSnapshot)
	{
		return Save(std::move(InPath), RecordType<T>(), std::move(InSnapshot));
	}

	void Drain();
	void ClearCache();

private:
	TAsyncResult<std::shared_ptr<const void>> Load(const std::filesystem::path& InPath,
	                                               const FRecordDescriptor& InType);
	TAsyncResult<bool> Save(std::filesystem::path InPath, const FRecordDescriptor& InType,
	                        std::shared_ptr<const void> InSnapshot);
	FIOService& IO;
	std::mutex Mutex;
	bool Loading{};
	bool Closing{};
	FCancellationToken Cancellation;
	std::vector<FAssetCodec> Codecs;
	std::map<std::pair<std::filesystem::path, std::string>, TAsyncResult<std::shared_ptr<const void>>> Cache;
	std::vector<FTaskHandle> Pending;
};
} // namespace Hyperion
