#pragma once
#include "Hyperion/Shaders/ShaderCompiler.h"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Hyperion
{
enum class ERHIBackend
{
	D3D12,
	Vulkan,
	Metal
};
enum class ERHIFeature
{
	Graphics,
	TextureSampling,
	ConcurrentRecording,
	Readback,
	RayTracing,
	MeshShaders,
	InstancedDrawing,
	Count
};

struct FRHIFeatureSupport
{
	// Hardware/driver support and usability through this RHI contract are distinct.
	bool bSupported{};
	bool bEnabled{};
};

struct FRHICapabilities
{
	ERHIBackend Backend = ERHIBackend::D3D12;
	EShaderFormat ShaderFormat = EShaderFormat::Dxil;
	std::string Adapter;
	std::uint32_t MaxRecordingContexts{};
	std::uint32_t MaxSampledTextures{};
	std::uint32_t MaxTextureDimension{};
	std::uint32_t MaxRegisterSpaces{};
	std::uint32_t MaxConstantBuffers{};
	std::uint32_t MaxSamplers{};
	std::uint32_t MaxReadBuffers{};
	std::uint32_t ConstantAlignment{};
	std::uint32_t MaxConstantRange{};
	std::uint32_t ResourceDescriptorCapacity{};
	std::uint32_t SamplerDescriptorCapacity{};
	std::uint32_t MaxAnisotropy{};
	bool bReadOnlyBuffers{};
	bool bVertexTextures{};
	bool bComparisonSamplers{};
	bool bSampledDepthTargets{};
	std::array<FRHIFeatureSupport, static_cast<std::size_t>(ERHIFeature::Count)> Features{};

	FRHIFeatureSupport QueryFeature(ERHIFeature InFeature) const;
};

struct FRHIDeviceDesc
{
	// Best effort; for D3D12 the first creation chooses the process-wide debug layer policy.
	bool bEnableDebug = true;
	// Required features must be enabled or creation fails. Optional features may stay disabled.
	std::vector<ERHIFeature> RequiredFeatures;
	std::vector<ERHIFeature> OptionalFeatures;
	std::uint32_t ResourceDescriptorCapacity = 4096;
	std::uint32_t SamplerDescriptorCapacity = 512;
};

std::string_view GetRHIBackendName(ERHIBackend InBackend);
ERHIBackend ParseRHIBackend(std::string_view InName);
void ValidateRequiredFeatures(const FRHIDeviceDesc& InDesc, const FRHICapabilities& InCapabilities);
} // namespace Hyperion
