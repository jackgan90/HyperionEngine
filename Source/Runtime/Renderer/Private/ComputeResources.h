#pragma once
#include "Hyperion/Renderer/ComputePass.h"
#include "Hyperion/Renderer/MaterialPreparation.h"
#include <set>

namespace Hyperion
{
struct FComputeResources
{
	struct FProgram
	{
		FComputeShader Description;
		FShaderArtifact Shader;
		std::vector<FMaterialProgramBinding> Bindings;
		std::vector<std::vector<FMaterialSemanticId>> MemberSemantics;
		std::vector<FMaterialSemanticId> ResourceSemantics;
		std::set<FMaterialSemanticId> ActiveSemantics;
		std::vector<std::weak_ptr<const void>> Owners;
	};

	struct FConstant
	{
		std::vector<std::byte> Bytes;
		FBufferSlice Slice;
		std::weak_ptr<const void> Owner;
	};

	std::vector<FProgram> Programs;
	std::vector<FConstant> Constants;
	FProgram& GetProgram(FShaderCompiler& InCompiler, EShaderFormat InFormat, const FComputePassDesc& InPass);
	void Collect();
};

FComputePassDesc SelectComputeParameters(const FComputePassDesc& InPass, const FComputeResources::FProgram& InProgram);
} // namespace Hyperion
