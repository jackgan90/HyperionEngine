#pragma once
#include "Hyperion/Renderer/MaterialPreparation.h"

namespace Hyperion
{
enum class ERenderMaterialStatus
{
	Preparing,
	Uploading,
	Ready,
	Failed,
	Retired
};

struct FRenderMaterialRecord;
struct FRenderResourceCoordinator;

// Thread-neutral lease for one frozen selection. Ordinary parameter revisions can share its static record.
class FRenderMaterial
{
public:
	~FRenderMaterial();
	ERenderMaterialStatus GetStatus() const;
	bool IsInterfaceReady() const;
	std::string GetError() const;
	std::shared_ptr<const FCompiledMaterialDefinition> GetCompiled() const;
	const std::shared_ptr<const FMaterialSnapshot>& GetSnapshot() const;

private:
	FRenderMaterial(std::shared_ptr<FRenderMaterialRecord> InRecord,
	                std::shared_ptr<const FMaterialSnapshot> InSnapshot, std::function<void()> InReleased);
	std::shared_ptr<FRenderMaterialRecord> Record;
	std::shared_ptr<const FMaterialSnapshot> Snapshot;
	std::function<void()> Released;
	friend struct FRenderResourceCoordinator;
	friend class FRenderResourceService;
};
} // namespace Hyperion
