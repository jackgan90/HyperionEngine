#include "RenderResourcesInternal.h"
#include <algorithm>

namespace Hyperion
{
std::shared_ptr<const void> FRenderResourceService::CreateScopeLifetime() const
{
	const std::weak_ptr<FRenderResourceCoordinator> Weak = Coordinator;
	return std::shared_ptr<const void>(new int(0),
	                                   [Weak](const void* InValue)
	                                   {
		                                   delete static_cast<const int*>(InValue);
		                                   if (auto Owner = Weak.lock())
		                                   {
			                                   Owner->Schedule();
		                                   }
	                                   });
}

namespace
{
bool IsResource(const FMaterialParameterType& InType)
{
	if (InType.Kind == EMaterialValueKind::Array)
	{
		return IsResource(InType.Members.front());
	}
	return InType.Kind == EMaterialValueKind::Texture2D || InType.Kind == EMaterialValueKind::ReadBuffer ||
	       InType.Kind == EMaterialValueKind::Sampler;
}

FMaterialParameterValues ResourceSignature(const FMaterialSnapshot& InSnapshot)
{
	if (!InSnapshot.Definition || !InSnapshot.Schema || InSnapshot.Identity == 0 || InSnapshot.Revision == 0)
	{
		throw std::invalid_argument("A render material requires an owned versioned snapshot");
	}
	// Validation includes numeric edits even though they do not invalidate static resources.
	const std::array<std::size_t, 0> NoRequiredParameters{};
	const auto Values = ResolveMaterialParameters(InSnapshot, {}, {}, {}, NoRequiredParameters);
	FMaterialParameterValues Result;
	for (const auto& Value : Values)
	{
		if (IsResource(Value.Value.Type))
		{
			Result.push_back(Value);
		}
	}
	std::sort(Result.begin(), Result.end(),
	          [](const auto& InA, const auto& InB)
	          {
		          return InA.Name < InB.Name;
	          });
	return Result;
}
} // namespace

std::shared_ptr<const FRenderMaterial> FRenderResourceCoordinator::MakeMaterialLease(
    FMaterialEntry& InEntry, std::shared_ptr<const FMaterialSnapshot> InSnapshot)
{
	std::erase_if(InEntry.Leases,
	              [](const auto& InLease)
	              {
		              return InLease.expired();
	              });
	for (const auto& Weak : InEntry.Leases)
	{
		if (auto Lease = Weak.lock(); Lease && Lease->GetSnapshot() == InSnapshot)
		{
			return Lease;
		}
	}
	const std::weak_ptr<FRenderResourceCoordinator> Weak = shared_from_this();
	auto Lease = std::shared_ptr<const FRenderMaterial>(new FRenderMaterial(InEntry.Record, std::move(InSnapshot),
	                                                                        [Weak]
	                                                                        {
		                                                                        if (auto Owner = Weak.lock())
		                                                                        {
			                                                                        Owner->Schedule();
		                                                                        }
	                                                                        }));
	InEntry.Leases.push_back(Lease);
	return Lease;
}

std::shared_ptr<const FRenderMaterial> FRenderResourceCoordinator::AcquireMaterial(
    std::shared_ptr<const FMaterialSnapshot> InSnapshot, std::shared_ptr<const FCompiledMaterialDefinition> InCompiled)
{
	if (!InSnapshot || bClosed)
	{
		throw std::invalid_argument("Cannot acquire an empty material or a closed session");
	}
	const auto Signature = ResourceSignature(*InSnapshot);
	if (InCompiled && InCompiled->Interface.Definition != InSnapshot->Definition)
	{
		throw std::invalid_argument("Foreign prepared material program");
	}
	if (InCompiled)
	{
		for (const auto& Pass : InCompiled->Passes)
		{
			if (Pass.Vertex.Format != Device.GetCapabilities().ShaderFormat ||
			    (!Pass.Pixel.Bytes.empty() && Pass.Pixel.Format != Device.GetCapabilities().ShaderFormat))
			{
				throw std::invalid_argument("Prepared material shader format is incompatible with the device");
			}
		}
	}
	auto& Program = Programs[{InSnapshot->Definition->GetIdentity(), InCompiled}];
	if (!Program || !Program->Error.empty())
	{
		auto PreparedProgram = std::make_shared<FMaterialProgramRecord>();
		PreparedProgram->Definition = InSnapshot->Definition;
		if (InCompiled)
		{
			PreparedProgram->Compiled = std::move(InCompiled);
		}
		else
		{
			PreparedProgram->Preparation = DispatchAsync<FCompiledMaterialDefinition>(
			    Tasks, {EDomain::Worker},
			    [Definition = PreparedProgram->Definition, ShaderCompiler = &Compiler,
			     Format = Device.GetCapabilities().ShaderFormat]
			    {
				    return CompileMaterialDefinition(*ShaderCompiler, Definition, Format);
			    });
		}
		Program = std::move(PreparedProgram);
	}
	for (auto& Entry : MaterialEntries)
	{
		const auto& Record = *Entry.Record;
		if (Record.Program == Program && Record.ResourceSignature == Signature &&
		    Record.Status != ERenderMaterialStatus::Failed && Record.Status != ERenderMaterialStatus::Retired)
		{
			return MakeMaterialLease(Entry, std::move(InSnapshot));
		}
	}
	auto Record = std::make_shared<FRenderMaterialRecord>(Tasks);
	Record->Owner = this;
	Record->Program = Program;
	Record->StaticSnapshot = std::make_shared<const FMaterialSnapshot>(*InSnapshot);
	Record->ResourceSignature = Signature;
	MaterialEntries.push_back({Record, {}});
	return MakeMaterialLease(MaterialEntries.back(), std::move(InSnapshot));
}

std::shared_ptr<const FRenderMaterial> FRenderResourceService::RequestMaterial(
    std::shared_ptr<const FMaterialSnapshot> InSnapshot)
{
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Main});
	std::shared_ptr<const FRenderMaterial> Result;
	{
		std::lock_guard Lock(Owner.Mutex);
		Result = Owner.AcquireMaterial(std::move(InSnapshot));
	}
	Owner.Schedule();
	return Result;
}
} // namespace Hyperion
