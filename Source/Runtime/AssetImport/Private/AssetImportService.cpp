#include "AssetImportInternal.h"
#include "Hyperion/Core/ContentHash.h"
#include <algorithm>
#include <cctype>

namespace Hyperion
{
std::string ImportPathString(const std::filesystem::path& InPath)
{
	const auto Utf8 = InPath.generic_u8string();
	return {reinterpret_cast<const char*>(Utf8.data()), Utf8.size()};
}

std::string ImportRelativePath(const std::filesystem::path& InPath, const std::filesystem::path& InBase)
{
	const auto Relative = InPath.lexically_relative(InBase);
	return ImportPathString(Relative.empty() ? InPath : Relative);
}

std::filesystem::path ImportPath(const std::filesystem::path& InPath)
{
	return std::filesystem::absolute(InPath).lexically_normal();
}

std::string ImportExtension(const std::filesystem::path& InPath)
{
	auto Extension = InPath.extension().string();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(),
	               [](unsigned char InCharacter)
	               {
		               return static_cast<char>(std::tolower(InCharacter));
	               });
	return Extension;
}

std::shared_ptr<const FBytes> FAssetImportContext::Read(const std::filesystem::path& InPath)
{
	Cancellation.Check();
	const auto PathValue = ImportPath(InPath);
	const auto BytesValue = IO.ReadAsync(PathValue, Cancellation).Get(IO.TaskSystem());
	const FAssetSource Source{ImportPathString(PathValue), ContentHash(*BytesValue)};
	const auto Existing = std::find_if(Sources.begin(), Sources.end(),
	                                   [&](const FAssetSource& InSource)
	                                   {
		                                   return InSource.Path == Source.Path;
	                                   });
	if (Existing != Sources.end() && Existing->Fingerprint != Source.Fingerprint)
	{
		throw std::runtime_error("Source changed during import: " + Source.Path);
	}
	if (Existing == Sources.end())
	{
		Sources.push_back(Source);
	}
	return BytesValue;
}

FAssetImportService::FAssetImportService(FIOService& InIO) : Impl(std::make_unique<FImpl>(InIO))
{
}

FAssetImportService::~FAssetImportService()
{
	Impl->Cancellation.Cancel();
	Drain();
}

void FAssetImportService::Register(FAssetImporter InImporter)
{
	std::lock_guard Lock(Impl->Mutex);
	if (Impl->bStarted || Impl->bClosing || InImporter.Id.empty() || !InImporter.Version || !InImporter.Type ||
	    !InImporter.Convert || InImporter.Extensions.empty())
	{
		throw std::logic_error("Register valid source importers before conversion");
	}
	ValidateRecordDescriptor(*InImporter.Type);
	for (auto& Extension : InImporter.Extensions)
	{
		std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		               [](unsigned char InCharacter)
		               {
			               return static_cast<char>(std::tolower(InCharacter));
		               });
		for (const auto& Existing : Impl->Importers)
		{
			if (Existing.Type->Id == InImporter.Type->Id &&
			    std::find(Existing.Extensions.begin(), Existing.Extensions.end(), Extension) !=
			        Existing.Extensions.end())
			{
				throw std::logic_error("Duplicate source importer");
			}
		}
	}
	Impl->Importers.push_back(std::move(InImporter));
}

FConvertedAsset FAssetImportService::FImpl::Convert(const std::filesystem::path& InPath, std::string_view InType)
{
	const auto Extension = ImportExtension(InPath);
	const auto Importer = std::find_if(Importers.begin(), Importers.end(),
	                                   [&](const FAssetImporter& InImporter)
	                                   {
		                                   return InImporter.Type->Id == InType &&
		                                          std::find(InImporter.Extensions.begin(), InImporter.Extensions.end(),
		                                                    Extension) != InImporter.Extensions.end();
	                                   });
	if (Importer == Importers.end())
	{
		throw std::runtime_error("No source importer for " + Extension + " and " + std::string(InType));
	}
	FAssetImportContext Context{IO, InPath, {}, Cancellation};
	Context.Bytes = Context.Read(InPath);
	try
	{
		auto Object = Importer->Convert(Context);
		if (!Object)
		{
			throw std::runtime_error("Source importer returned a null object");
		}
		const auto Weight = EncodeArchive(WriteRecord(*Importer->Type, Object.get())).size() * 2;
		std::sort(Context.Sources.begin(), Context.Sources.end());
		return {std::make_shared<const FRecordDescriptor>(*Importer->Type),
		        std::move(Object),
		        std::move(Context.Sources),
		        Importer->Id,
		        Importer->Version,
		        Weight,
		        Extension == ".hasset" ? std::optional<FAssetHeader>(DecodeAsset(Context.Bytes).Header) : std::nullopt};
	}
	catch (const std::exception& Error)
	{
		throw std::runtime_error(ImportPathString(InPath) + ": " + Error.what());
	}
}

void FAssetImportService::FImpl::ScheduleTrim(FTaskHandle InTask)
{
	const auto Cleanup = IO.TaskSystem().Dispatch({EDomain::Worker},
	                                              [this, InTask]
	                                              {
		                                              try
		                                              {
			                                              IO.TaskSystem().Wait(InTask);
		                                              }
		                                              catch (...)
		                                              {
		                                              }
		                                              std::lock_guard Lock(Mutex);
		                                              Trim();
	                                              });
	Pending.push_back(Cleanup);
}

void FAssetImportService::FImpl::Trim()
{
	std::erase_if(Pending,
	              [](const FTaskHandle& InTask)
	              {
		              return InTask.Ready();
	              });
	std::erase_if(Publications,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Ready();
	              });
	std::size_t Bytes{};
	std::size_t Completed{};
	for (const auto& [Key, Request] : Cache)
	{
		if (Request.Ready())
		{
			++Completed;
			try
			{
				Bytes += Request.GetReady()->RetainedBytes;
			}
			catch (...)
			{
			}
		}
	}
	for (auto It = Cache.begin(); It != Cache.end() && (Completed > 16 || Bytes > 128u * 1024u * 1024u);)
	{
		if (It->second.Ready())
		{
			try
			{
				Bytes -= It->second.GetReady()->RetainedBytes;
			}
			catch (...)
			{
			}
			--Completed;
			It = Cache.erase(It);
		}
		else
		{
			++It;
		}
	}
}

TAsyncResult<FConvertedAsset> FAssetImportService::Load(const std::filesystem::path& InPath, std::string_view InType)
{
	const auto Path = ImportPath(InPath);
	const auto Key = std::make_pair(Path, std::string(InType));
	std::lock_guard Lock(Impl->Mutex);
	if (Impl->bClosing)
	{
		throw std::logic_error("Import service is closing");
	}
	Impl->bStarted = true;
	Impl->Trim();
	if (const auto It = Impl->Cache.find(Key); It != Impl->Cache.end())
	{
		if (!It->second.Ready())
		{
			return It->second;
		}
		try
		{
			(void)It->second.GetReady();
			return It->second;
		}
		catch (...)
		{
			Impl->Cache.erase(It);
		}
	}
	if (Impl->Pending.size() >= 256)
	{
		throw std::runtime_error("Source import in-flight limit exceeded");
	}
	Impl->Pending.reserve(Impl->Pending.size() + 2);
	auto Result = DispatchAsync<FConvertedAsset>(
	    Impl->IO.TaskSystem(), {EDomain::Worker},
	    [State = Impl.get(), Path, Type = std::string(InType)]
	    {
		    return State->Convert(Path, Type);
	    },
	    Impl->Cancellation);
	Impl->Pending.push_back(Result.Task());
	Impl->Cache.emplace(Key, Result);
	Impl->ScheduleTrim(Result.Task());
	return Result;
}

void FAssetImportService::ClearCache()
{
	std::lock_guard Lock(Impl->Mutex);
	std::erase_if(Impl->Cache,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Ready();
	              });
	Impl->Trim();
}

void FAssetImportService::Drain()
{
	std::vector<FTaskHandle> Work;
	{
		std::lock_guard Lock(Impl->Mutex);
		Impl->bClosing = true;
		Work = Impl->Pending;
	}
	for (const auto& Task : Work)
	{
		try
		{
			Impl->IO.TaskSystem().Wait(Task);
		}
		catch (...)
		{
		}
	}
	std::lock_guard Lock(Impl->Mutex);
	Impl->Pending.clear();
	Impl->Cache.clear();
	Impl->Publications.clear();
}
} // namespace Hyperion
