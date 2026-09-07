#include "RenderResourcesInternal.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
std::vector<std::optional<FMaterialValue>> StaticValues(const FMaterialSnapshot& InSnapshot,
                                                        const FCompiledMaterialDefinition& InCompiled)
{
	FMaterialSnapshot Snapshot = InSnapshot;
	Snapshot.Schema = InCompiled.Interface.Schema;
	std::vector<std::size_t> Required;
	const auto& Parameters = Snapshot.Schema->GetParameters();
	for (const auto& Pass : InCompiled.Passes)
	{
		for (const auto& Binding : Pass.Bindings)
		{
			if (Binding.ResourceParameter &&
			    Parameters[*Binding.ResourceParameter].Source == EMaterialParameterSource::Manual)
			{
				Required.push_back(*Binding.ResourceParameter);
			}
		}
	}
	const auto Values = ResolveMaterialParameters(Snapshot, {}, {}, {}, Required);
	std::vector<std::optional<FMaterialValue>> Result(Parameters.size());
	for (const auto& Value : Values)
	{
		Result[Snapshot.Schema->Find(Value.Name).Index] = Value.Value;
	}
	return Result;
}

void ProcessProgram(FMaterialProgramRecord& InProgram)
{
	if (InProgram.Compiled || !InProgram.Error.empty() || !InProgram.Preparation.Ready())
	{
		return;
	}
	try
	{
		InProgram.Compiled = InProgram.Preparation.GetReady();
	}
	catch (const std::exception& Error)
	{
		InProgram.Error = Error.what();
	}
	catch (...)
	{
		InProgram.Error = "Unknown material program preparation failure";
	}
	InProgram.Preparation = {};
}
} // namespace

void FRenderResourceCoordinator::EnsureMaterialCaches()
{
	Tasks.Require({EDomain::Rhi, 0});
	if (!MaterialGpu)
	{
		MaterialGpu = std::make_unique<FMaterialGpuCache>(Device);
	}
	if (!MaterialConstants)
	{
		MaterialConstants = std::make_unique<FMaterialConstantCache>(Device);
	}
}

void FRenderResourceCoordinator::PrepareMaterialResources(FRenderMaterialRecord& InRecord)
{
	EnsureMaterialCaches();
	const auto Values = StaticValues(*InRecord.StaticSnapshot, *InRecord.Compiled);
	bool bReady = true;
	std::vector<FMaterialResourceBindings> Bindings;
	for (const auto& Pass : InRecord.Compiled->Passes)
	{
		auto Binding = MaterialGpu->BindResources(Pass, Values, {InRecord.GpuLifetime}, true);
		bReady &= Binding.bReady;
		Bindings.push_back(std::move(Binding));
	}
	InRecord.StaticBindings = std::move(Bindings);
	InRecord.Publish(bReady ? ERenderMaterialStatus::Ready : ERenderMaterialStatus::Uploading);
}

bool FRenderResourceCoordinator::ProcessMaterials()
{
	Tasks.Require({EDomain::Rhi, 0});
	bool bPending = false;
	for (auto& [Identity, Program] : Programs)
	{
		if (Program)
		{
			ProcessProgram(*Program);
		}
	}
	for (auto Iterator = MaterialEntries.begin(); Iterator != MaterialEntries.end();)
	{
		auto& Entry = *Iterator;
		auto& Record = *Entry.Record;
		std::erase_if(Entry.Leases,
		              [](const auto& InLease)
		              {
			              return InLease.expired();
		              });
		if (Entry.Leases.empty())
		{
			Record.Release();
			Iterator = MaterialEntries.erase(Iterator);
			continue;
		}
		if (Record.Status == ERenderMaterialStatus::Failed)
		{
			Record.StaticBindings.clear();
			Record.GpuLifetime.reset();
		}
		try
		{
			if (Record.Status == ERenderMaterialStatus::Preparing)
			{
				if (!Record.Program->Error.empty())
				{
					throw std::runtime_error(Record.Program->Error);
				}
				if (Record.Program->Compiled)
				{
					{
						std::lock_guard Lock(Record.Publication);
						Record.Compiled = Record.Program->Compiled;
					}
					PrepareMaterialResources(Record);
				}
			}
			else if (Record.Status == ERenderMaterialStatus::Uploading)
			{
				PrepareMaterialResources(Record);
			}
		}
		catch (const std::exception& Error)
		{
			Record.StaticBindings.clear();
			Record.GpuLifetime.reset();
			Record.Publish(ERenderMaterialStatus::Failed, Error.what());
		}
		catch (...)
		{
			Record.StaticBindings.clear();
			Record.GpuLifetime.reset();
			Record.Publish(ERenderMaterialStatus::Failed, "Unknown material resource failure");
		}
		bPending |=
		    Record.Status == ERenderMaterialStatus::Preparing || Record.Status == ERenderMaterialStatus::Uploading;
		++Iterator;
	}
	for (auto Iterator = Programs.begin(); Iterator != Programs.end();)
	{
		const auto& Program = Iterator->second;
		const bool bReady = !Program || Program->Compiled || !Program->Error.empty();
		if ((!Program || Program.use_count() == 1) && bReady)
		{
			Iterator = Programs.erase(Iterator);
		}
		else
		{
			bPending |= !bReady;
			++Iterator;
		}
	}
	if (MaterialGpu)
	{
		bPending |= MaterialGpu->Collect();
		Stats.Materials = MaterialGpu->Statistics();
	}
	if (MaterialConstants)
	{
		bPending |= MaterialConstants->Collect();
		Stats.Constants = MaterialConstants->Statistics();
	}
	return bPending;
}
} // namespace Hyperion
