#pragma once
#include "Hyperion/Materials/Material.h"
#include "Hyperion/Shaders/ShaderCompiler.h"
#include "Hyperion/Tasks/AsyncResult.h"

namespace Hyperion
{
enum class EMaterialEngineBindingMode
{
	Automatic,
	Explicit
};

enum class EMaterialExecutionMode
{
	Ordinary,
	Instanced
};

struct FMaterialVariantRequest
{
	std::string Usage = "Forward";
	std::string Name = "Default";
	std::vector<FShaderDefine> Defines;
	EMaterialExecutionMode ExecutionMode = EMaterialExecutionMode::Ordinary;
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
	EShaderStageMask Stages = EShaderStageMask::None;
	std::vector<FMaterialBindingMember> Members;
	std::optional<std::size_t> ResourceParameter;
	std::uint32_t InstanceStride{};
	std::uint32_t InstanceCapacity{};
};

struct FCompiledMaterialPass
{
	std::string Usage;
	std::string Variant;
	std::vector<FShaderDefine> VariantDefines;
	FShaderArtifact Vertex;
	FShaderArtifact Pixel;
	std::vector<FMaterialProgramBinding> Bindings;
	std::vector<std::size_t> ActiveParameters;
	std::uint32_t InstanceCapacity = 1;
	EMaterialExecutionMode ExecutionMode = EMaterialExecutionMode::Ordinary;
};

struct FCompiledMaterialDefinition
{
	FPreparedMaterialInterface Interface;
	std::vector<FCompiledMaterialPass> Passes;
	std::string Key;
	std::vector<std::string> InstanceDiagnostics;
	// Each usage has at most one instance candidate, regardless of its name.
	const FCompiledMaterialPass* FindInstancePass(std::string_view InUsage = "Forward") const;
	const FCompiledMaterialPass& GetInstancePass(std::string_view InUsage = "Forward") const;
	const FCompiledMaterialPass& GetDrawPass(std::string_view InUsage, EMaterialExecutionMode InMode) const;
	// Session consumers select the ordinary Default pass; explicit names remain available for inspection.
	const FCompiledMaterialPass& GetPass(std::string_view InUsage = "Forward") const;
	const FCompiledMaterialPass& GetPass(std::string_view InUsage, std::string_view InVariant) const;
};

FMaterialParameterType GetMaterialParameterType(const FShaderMember& InMember);
// Returns a record-shaped reflection view, retaining full native reflection in the compiled binding.
FShaderBinding PrepareMaterialInstanceBinding(const FShaderBinding& InResource, const FMaterialPass& InPass,
                                              FMaterialProgramBinding& OutBinding);
std::vector<FMaterialProgramBinding> MergeMaterialBindings(std::vector<FMaterialProgramBinding> InBindings);

// Synchronous Worker preparation, also usable by existing resource-production jobs.
FCompiledMaterialDefinition CompileMaterialDefinition(
    FShaderCompiler& InCompiler, std::shared_ptr<const FMaterialDefinition> InDefinition, EShaderFormat InFormat,
    std::vector<FMaterialVariantRequest> InVariants = {},
    EMaterialEngineBindingMode InEngineMode = EMaterialEngineBindingMode::Automatic);

// Main starts preparation. The captured compiler/definition stay alive until Worker completion.
TAsyncResult<FCompiledMaterialDefinition> PrepareMaterialDefinition(
    FTaskSystem& InTasks, std::shared_ptr<FShaderCompiler> InCompiler,
    std::shared_ptr<const FMaterialDefinition> InDefinition, EShaderFormat InFormat,
    std::vector<FMaterialVariantRequest> InVariants = {}, FCancellationToken InCancellation = {});
} // namespace Hyperion
