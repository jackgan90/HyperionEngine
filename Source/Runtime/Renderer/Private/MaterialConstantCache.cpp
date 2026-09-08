#include "Hyperion/Renderer/MaterialConstantCache.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/MaterialPacking.h"
#include <algorithm>
#include <map>
#include <set>
#include <thread>

namespace Hyperion
{
namespace
{
struct FConstantKey
{
	EShaderFormat Format{};
	std::uint32_t Size{};
	std::vector<FShaderMember> Layout;
	std::vector<std::string> Mapping;
	std::vector<std::shared_ptr<const FMaterialValue>> Values;
	std::vector<std::pair<EMaterialScope, FMaterialScopeKey>> Scopes;
};

std::uint64_t HashBinding(const FMaterialProgramBinding& InBinding, EShaderFormat InFormat,
                          const FMaterialParameterSchema& InSchema, const FResolvedMaterialParameters& InParameters)
{
	// Full structured equality follows every hash hit. The hash only selects a small candidate bucket.
	std::uint64_t Hash = 14695981039346656037ULL;
	const auto Add = [&](std::uint64_t InValue)
	{
		Hash = (Hash ^ InValue) * 1099511628211ULL;
	};
	Add(InBinding.Resource.ByteSize);
	Add(static_cast<unsigned>(InFormat));
	std::uint32_t Dependencies{};
	for (const auto& Member : InBinding.Members)
	{
		Add(Member.Layout.Offset);
		Add(Member.Layout.Size);
		Add(Member.Layout.Rows);
		Add(Member.Layout.Columns);
		Dependencies |= InParameters.Dependencies.at(Member.ParameterIndex);
	}
	for (const auto& Member : InBinding.Members)
	{
		const auto& Parameter = InSchema.GetParameters().at(Member.ParameterIndex);
		Add(Parameter.Name.size() + 1 + Parameter.Semantic.size());
		for (const unsigned char Character : Parameter.Name)
		{
			Add(Character);
		}
		Add(0);
		for (const unsigned char Character : Parameter.Semantic)
		{
			Add(Character);
		}
	}
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if ((Dependencies & (1U << Scope)) == 0)
		{
			continue;
		}
		const auto& Input = InParameters.Scopes[Scope];
		const auto& Key = Input.Key;
		if (!Input.Lifetime || Key.Identity == 0 || Key.Revision == 0)
		{
			throw std::invalid_argument("Material constant dependency requires an owned scope identity and revision");
		}
		Add(static_cast<unsigned>(Scope));
		Add(Key.Identity);
		Add(Key.Revision);
		for (const auto Qualifier : Key.Qualifiers)
		{
			Add(Qualifier);
		}
	}
	return Hash;
}

bool MatchesBinding(const FConstantKey& InKey, const FMaterialProgramBinding& InBinding, EShaderFormat InFormat,
                    const FMaterialParameterSchema& InSchema, const FResolvedMaterialParameters& InParameters)
{
	if (InKey.Format != InFormat || InKey.Size != InBinding.Resource.ByteSize ||
	    InKey.Layout.size() != InBinding.Members.size())
	{
		return false;
	}
	std::uint32_t Dependencies{};
	for (std::size_t Index = 0; Index < InBinding.Members.size(); ++Index)
	{
		const auto& Member = InBinding.Members[Index];
		const auto& Parameter = InSchema.GetParameters().at(Member.ParameterIndex);
		const std::string_view Mapping = InKey.Mapping[Index];
		if (InKey.Layout[Index] != Member.Layout ||
		    !SameMaterialValue(InKey.Values[Index], InParameters.Values.at(Member.ParameterIndex)) ||
		    Mapping.size() != Parameter.Name.size() + 1 + Parameter.Semantic.size() ||
		    !Mapping.starts_with(Parameter.Name) || Mapping[Parameter.Name.size()] != '\0' ||
		    Mapping.substr(Parameter.Name.size() + 1) != Parameter.Semantic)
		{
			return false;
		}
		Dependencies |= InParameters.Dependencies.at(Member.ParameterIndex);
	}
	std::size_t Index{};
	for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
	{
		if ((Dependencies & (1U << Scope)) != 0)
		{
			if (Index >= InKey.Scopes.size() || InKey.Scopes[Index].first != static_cast<EMaterialScope>(Scope) ||
			    InKey.Scopes[Index].second != InParameters.Scopes[Scope].Key)
			{
				return false;
			}
			++Index;
		}
	}
	return Index == InKey.Scopes.size();
}

struct FCacheEntry
{
	FConstantKey Key;
	FBufferSlice Slice;
	std::vector<std::weak_ptr<const void>> Owners;

	bool IsExpired() const
	{
		return std::any_of(Owners.begin(), Owners.end(),
		                   [](const auto& InOwner)
		                   {
			                   return InOwner.expired();
		                   });
	}
};

FCacheEntry MakeEntry(const FMaterialProgramBinding& InBinding, EShaderFormat InFormat,
                      const FMaterialParameterSchema& InSchema, const FResolvedMaterialParameters& InParameters)
{
	FCacheEntry Result;
	auto& Key = Result.Key;
	Key.Format = InFormat;
	Key.Size = InBinding.Resource.ByteSize;
	std::uint32_t Dependencies{};
	for (const auto& Member : InBinding.Members)
	{
		Key.Layout.push_back(Member.Layout);
		const auto& Declaration = InSchema.GetParameters().at(Member.ParameterIndex);
		Key.Mapping.push_back(Declaration.Name + std::string(1, '\0') + Declaration.Semantic);
		Key.Values.push_back(InParameters.Values.at(Member.ParameterIndex));
		Dependencies |= InParameters.Dependencies.at(Member.ParameterIndex);
	}
	for (std::size_t Index = 0; Index < MaterialScopeCount; ++Index)
	{
		if ((Dependencies & (1U << Index)) == 0)
		{
			continue;
		}
		const auto& Input = InParameters.Scopes[Index];
		if (Input.Key.Identity == 0 || Input.Key.Revision == 0 || !Input.Lifetime)
		{
			throw std::invalid_argument("Material constant dependency requires an owned scope identity and revision");
		}
		Key.Scopes.push_back({static_cast<EMaterialScope>(Index), Input.Key});
		Result.Owners.push_back(Input.Lifetime);
	}
	if (Result.Owners.empty())
	{
		Result.Owners.push_back(InParameters.Scopes[static_cast<std::size_t>(EMaterialScope::Material)].Lifetime);
	}
	return Result;
}
} // namespace

struct FMaterialConstantCache::FImpl
{
	struct FPage
	{
		FBuffer Buffer;
		std::uint32_t End{};
		bool bTransient{};
	};

	IRHIDevice& Device;
	std::thread::id Owner = std::this_thread::get_id();
	std::uint32_t PageSize;
	std::map<std::uint64_t, std::vector<FCacheEntry>> Entries;
	std::vector<FPage> Pages;
	FMaterialConstantStats Stats;

	FImpl(IRHIDevice& InDevice, std::uint32_t InPageSize) : Device(InDevice), PageSize(InPageSize)
	{
	}

	void CheckOwner() const
	{
		if (Owner != std::this_thread::get_id())
		{
			throw std::logic_error("Material constant cache requires its RHI owner");
		}
	}

	FBufferSlice Publish(std::span<const std::byte> InData, bool bInTransient)
	{
		const auto Alignment = Device.GetCapabilities().ConstantAlignment;
		const auto Extent = static_cast<std::uint32_t>((InData.size() + Alignment - 1) / Alignment * Alignment);
		auto Page = std::find_if(Pages.begin(), Pages.end(),
		                         [&](const FPage& InPage)
		                         {
			                         return (InPage.End == 0 || InPage.bTransient == bInTransient) &&
			                                Extent <= PageSize - InPage.End;
		                         });
		if (Page == Pages.end())
		{
			Pages.push_back({Device.CreateBuffer({PageSize, BufferUsage(ERHIBufferUsage::Constant)}), 0, bInTransient});
			Page = std::prev(Pages.end());
			++Stats.PagesCreated;
		}
		Page->bTransient = bInTransient;
		FBufferSlice Result = Device.PublishConstantSlice(Page->Buffer, Page->End, InData);
		Page->End += Extent;
		Stats.UploadBytes += InData.size();
		return Result;
	}

	FBufferSlice Bind(const FMaterialProgramBinding& InBinding, EShaderFormat InFormat,
	                  const FMaterialParameterSchema& InSchema, const FResolvedMaterialParameters& InParameters)
	{
		auto& Bucket = Entries[HashBinding(InBinding, InFormat, InSchema, InParameters)];
		for (const auto& Existing : Bucket)
		{
			if (!Existing.IsExpired() && MatchesBinding(Existing.Key, InBinding, InFormat, InSchema, InParameters))
			{
				++Stats.Reuses;
				return Existing.Slice;
			}
		}
		FCacheEntry Entry = MakeEntry(InBinding, InFormat, InSchema, InParameters);
		bool bTransient = false;
		for (const auto& [Scope, Key] : Entry.Key.Scopes)
		{
			bTransient |= Scope == EMaterialScope::Frame || Scope == EMaterialScope::Draw;
			++Stats.ScopePacks[static_cast<std::size_t>(Scope)];
		}
		const auto Data = PackMaterialConstants(InBinding, InParameters.Values);
		if (Data.size() > PageSize)
		{
			throw std::invalid_argument("Material constant block exceeds page capacity");
		}
		Entry.Slice = Publish(Data, bTransient);
		++Stats.Packs;
		Bucket.push_back(std::move(Entry));
		return Bucket.back().Slice;
	}
};

FMaterialConstantCache::FMaterialConstantCache(IRHIDevice& InDevice, std::uint32_t InPageSize)
    : Impl(std::make_unique<FImpl>(InDevice, InPageSize))
{
	const auto Alignment = InDevice.GetCapabilities().ConstantAlignment;
	if (Alignment == 0 || InPageSize == 0 || InPageSize % Alignment != 0)
	{
		throw std::invalid_argument("Invalid material constant page alignment or capacity");
	}
}

FMaterialConstantCache::~FMaterialConstantCache() = default;

std::vector<FConstantBinding> FMaterialConstantCache::Bind(const FCompiledMaterialPass& InPass,
                                                           const FMaterialParameterSchema& InSchema,
                                                           const FResolvedMaterialParameters& InParameters)
{
	HYP_PERF_SCOPE_C(Detail, BindMaterialConstants);
	Impl->CheckOwner();
	std::vector<FConstantBinding> Result;
	for (std::uint32_t Index = 0; Index < InPass.Bindings.size(); ++Index)
	{
		const auto& Binding = InPass.Bindings[Index];
		if (Binding.Resource.Kind == EBindingKind::UniformBuffer)
		{
			Result.push_back({Index, Impl->Bind(Binding, InPass.Vertex.Format, InSchema, InParameters)});
		}
	}
	return Result;
}

bool FMaterialConstantCache::Collect()
{
	Impl->CheckOwner();
	for (auto Iterator = Impl->Entries.begin(); Iterator != Impl->Entries.end();)
	{
		std::erase_if(Iterator->second,
		              [](const FCacheEntry& InEntry)
		              {
			              return InEntry.IsExpired();
		              });
		if (Iterator->second.empty())
		{
			Iterator = Impl->Entries.erase(Iterator);
		}
		else
		{
			++Iterator;
		}
	}
	bool bKeptIdle = false;
	std::erase_if(Impl->Pages,
	              [&](FImpl::FPage& InPage)
	              {
		              if (InPage.Buffer.Payload.use_count() != 1)
		              {
			              return false;
		              }
		              if (bKeptIdle)
		              {
			              return true;
		              }
		              bKeptIdle = true;
		              if (InPage.End != 0)
		              {
			              Impl->Device.ResetConstantBuffer(InPage.Buffer);
			              InPage.End = 0;
			              ++Impl->Stats.PagesReset;
		              }
		              return false;
	              });
	std::set<const IRHIBuffer*> UsedPages;
	for (const auto& [Hash, Bucket] : Impl->Entries)
	{
		for (const auto& Entry : Bucket)
		{
			UsedPages.insert(Entry.Slice.Buffer.Payload.get());
		}
	}
	for (const auto& Page : Impl->Pages)
	{
		// Live scopes will notify on retirement. Poll only pages whose CPU entries are gone but packets remain.
		if (!UsedPages.contains(Page.Buffer.Payload.get()) && Page.Buffer.Payload.use_count() > 1)
		{
			return true;
		}
	}
	return false;
}

void FMaterialConstantCache::Clear()
{
	Impl->CheckOwner();
	Impl->Entries.clear();
	Collect();
}

bool FMaterialConstantCache::CanRelease() const
{
	Impl->CheckOwner();
	return std::all_of(Impl->Pages.begin(), Impl->Pages.end(),
	                   [](const FImpl::FPage& InPage)
	                   {
		                   return InPage.Buffer.Payload.use_count() == 1;
	                   });
}

FMaterialConstantStats FMaterialConstantCache::Statistics() const
{
	Impl->CheckOwner();
	auto Result = Impl->Stats;
	Result.LivePages = Impl->Pages.size();
	for (const auto& [Hash, Bucket] : Impl->Entries)
	{
		Result.CachedBlocks += Bucket.size();
	}
	return Result;
}
} // namespace Hyperion
