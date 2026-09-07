#include "RenderResourcesInternal.h"

namespace Hyperion
{
FRenderMaterial::FRenderMaterial(std::shared_ptr<FRenderMaterialRecord> InRecord,
                                 std::shared_ptr<const FMaterialSnapshot> InSnapshot, std::function<void()> InReleased)
    : Record(std::move(InRecord)), Snapshot(std::move(InSnapshot)), Released(std::move(InReleased))
{
}

FRenderMaterial::~FRenderMaterial()
{
	Record.reset();
	Released();
}

ERenderMaterialStatus FRenderMaterial::GetStatus() const
{
	std::lock_guard Lock(Record->Publication);
	return Record->Status;
}

bool FRenderMaterial::IsInterfaceReady() const
{
	return GetCompiled() != nullptr;
}

std::string FRenderMaterial::GetError() const
{
	std::lock_guard Lock(Record->Publication);
	return Record->Error;
}

std::shared_ptr<const FCompiledMaterialDefinition> FRenderMaterial::GetCompiled() const
{
	std::lock_guard Lock(Record->Publication);
	return Record->Compiled;
}

const std::shared_ptr<const FMaterialSnapshot>& FRenderMaterial::GetSnapshot() const
{
	return Snapshot;
}

FRenderMaterialRecord::~FRenderMaterialRecord()
{
	if (!StaticBindings.empty() && !Tasks.IsCurrent({EDomain::Rhi, 0}))
	{
		std::terminate();
	}
}

void FRenderMaterialRecord::Publish(ERenderMaterialStatus InStatus, std::string InError)
{
	std::lock_guard Lock(Publication);
	Status = InStatus;
	Error = std::move(InError);
}

void FRenderMaterialRecord::Release()
{
	Tasks.Require({EDomain::Rhi, 0});
	StaticBindings.clear();
	GpuLifetime.reset();
	Publish(ERenderMaterialStatus::Retired);
}

std::shared_ptr<const FRenderMaterial> FRenderResource::GetMaterial(std::uint32_t InSection) const
{
	std::lock_guard Lock(Record->Publication);
	if (Record->Status != ERenderResourceStatus::Ready || !Record->Description ||
	    InSection >= Record->Description->Sections.size())
	{
		return {};
	}
	const auto Index = Record->Description->Sections[InSection].Material;
	return Index < Record->Materials.size() ? Record->Materials[Index] : nullptr;
}
} // namespace Hyperion
