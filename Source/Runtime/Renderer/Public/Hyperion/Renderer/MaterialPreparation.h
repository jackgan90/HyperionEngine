#pragma once
#include "Hyperion/Materials/Material.h"
#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Hyperion/Tasks/AsyncResult.h"

namespace Hyperion
{
struct FMaterialVariantRequest
{
	std::string Usage = "Forward";
	std::string Name = "Default";
	std::vector<FShaderDefine> Defines;
};

struct FMaterialBindingMember
{
	FShaderMember Layout;
	std::size_t ParameterIndex{};
	bool operator==(const FMaterialBindingMember&) const = default;
};

struct FMaterialProgramBinding
{
	FShaderBinding Resource;
	std::uint32_t Stages{}; // Bit 0: VS, bit 1: PS.
	std::vector<FMaterialBindingMember> Members;
	std::optional<std::size_t> ResourceParameter;
};

struct FCompiledMaterialPass
{
	std::string Usage;
	std::string Variant;
	FShaderArtifact Vertex;
	FShaderArtifact Pixel;
	std::vector<FMaterialProgramBinding> Bindings;
	std::vector<std::size_t> ActiveParameters;
};

struct FCompiledMaterialDefinition
{
	FPreparedMaterialInterface Interface;
	std::vector<FCompiledMaterialPass> Passes;
	std::string Key;
	const FCompiledMaterialPass& GetPass(std::string_view InUsage = "Forward",
	                                     std::string_view InVariant = "Default") const;
};

FMaterialParameterType GetMaterialParameterType(const FShaderMember& InMember);
std::vector<FMaterialProgramBinding> MergeMaterialBindings(std::vector<FMaterialProgramBinding> InBindings);

// Synchronous Worker preparation, also usable by existing resource-production jobs.
FCompiledMaterialDefinition CompileMaterialDefinition(FShaderCompiler& InCompiler,
                                                      std::shared_ptr<const FMaterialDefinition> InDefinition,
                                                      EShaderFormat InFormat,
                                                      std::vector<FMaterialVariantRequest> InVariants = {});

// Main starts preparation. The captured compiler/definition stay alive until Worker completion.
TAsyncResult<FCompiledMaterialDefinition> PrepareMaterialDefinition(
    FTaskSystem& InTasks, std::shared_ptr<FShaderCompiler> InCompiler,
    std::shared_ptr<const FMaterialDefinition> InDefinition, EShaderFormat InFormat,
    std::vector<FMaterialVariantRequest> InVariants = {}, FCancellationToken InCancellation = {});
} // namespace Hyperion
