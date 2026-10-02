#include "Hyperion/RHI/RHIDevice.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <array>

using namespace Hyperion;

namespace
{
// Freeze the old acceptance set without deriving the oracle from shared helpers.
constexpr std::array<std::uint32_t, 28> AcceptedNativeUsages{1,  2,  3,  4,  8,  9,  10, 11, 16, 17, 18, 19,  24,  25,
                                                             26, 27, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120};

void CheckCreate(IRHIDevice& InDevice, FBufferDesc InDescription, bool bInAccepted,
                 std::span<const std::byte> InBytes = {})
{
	bool bRejected = false;
	try
	{
		const auto Buffer = InDevice.CreateBuffer(InDescription, InBytes);
		HYP_CHECK(Buffer);
		HYP_CHECK(Buffer.Payload->GetInfo().Size == InDescription.Size);
		HYP_CHECK(Buffer.Payload->GetInfo().Usage == InDescription.Usage);
	}
	catch (const std::invalid_argument& Error)
	{
		const std::string Message = Error.what();
		HYP_CHECK(Message == "Invalid typed buffer size, usage or initial data" ||
		          Message == "Storage buffer requires exclusive shader usage and four-byte alignment" ||
		          Message == "Constant pages require exclusive constant usage and 256-byte alignment");
		bRejected = true;
	}
	if (bRejected == bInAccepted)
	{
		throw std::runtime_error("Unexpected native buffer acceptance: usage=" + std::to_string(InDescription.Usage) +
		                         ", size=" + std::to_string(InDescription.Size));
	}
}

void CheckCreationMatrix(IRHIDevice& InDevice)
{
	for (std::uint32_t Usage = 0; Usage < 128; ++Usage)
	{
		CheckCreate(InDevice, {256, Usage},
		            std::ranges::find(AcceptedNativeUsages, Usage) != AcceptedNativeUsages.end());
	}
	for (std::uint32_t Bit = 7; Bit < 32; ++Bit)
	{
		const std::uint32_t Unknown = std::uint32_t{1} << Bit;
		for (const std::uint32_t Usage : {Unknown, Unknown | 8U, Unknown | 120U})
		{
			CheckCreate(InDevice, {256, Usage}, false);
		}
	}
	CheckCreate(InDevice, {0, 8}, false);
	CheckCreate(InDevice, {1, 8}, true);
	CheckCreate(InDevice, {4, 32}, true);
	CheckCreate(InDevice, {6, 32}, false);
	CheckCreate(InDevice, {4, 64}, true);
	CheckCreate(InDevice, {6, 64}, false);
	CheckCreate(InDevice, {4, 4}, false);
	CheckCreate(InDevice, {260, 4}, false);
	const std::array<std::byte, 257> Excess{};
	CheckCreate(InDevice, {256, 8}, false, Excess);
	CheckCreate(InDevice, {256, 32}, false, Excess);
	CheckCreate(InDevice, {256, 4}, false, Excess);
}

struct FAccessCase
{
	std::uint32_t Usage;
	EResourceState State;
	bool bCompute;
	bool bAccepted;
};

void CheckAccess(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, FSize InSize, const FAccessCase& InCase)
{
	const auto Buffer = InDevice.CreateBuffer({256, InCase.Usage});
	FPassCommands Commands;
	Commands.Name = "Buffer usage access compatibility";
	Commands.bCompute = InCase.bCompute;
	Commands.BufferAccesses = {{{Buffer, ERHIBufferViewKind::Raw, 0, 256, 0}, InCase.State}};
	InSwapchain.BeginFrame(InSize);
	bool bRejected = false;
	try
	{
		HYP_CHECK(InSwapchain.Record(0, Commands).Payload);
	}
	catch (const std::invalid_argument& Error)
	{
		HYP_CHECK(std::string(Error.what()) == "Invalid buffer access range, state or usage");
		bRejected = true;
	}
	InSwapchain.CancelFrame();
	if (bRejected == InCase.bAccepted)
	{
		throw std::runtime_error("Unexpected native buffer access acceptance: " + std::to_string(InCase.Usage));
	}
}

void CheckStorageReadback(IRHIDevice& InDevice, const FBuffer& InBuffer, bool bInStorage)
{
	bool bRejected = false;
	try
	{
		const auto Bytes =
		    InDevice.ReadBuffer({InBuffer, ERHIBufferViewKind::Raw, 0, 4, 0}, EResourceState::ShaderRead);
		const std::array Expected{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
		HYP_CHECK(std::ranges::equal(Bytes, Expected));
	}
	catch (const std::invalid_argument& Error)
	{
		HYP_CHECK(std::string(Error.what()) == "Readback requires a valid storage buffer range");
		bRejected = true;
	}
	HYP_CHECK(bRejected != bInStorage);
}

void CheckStorageTransition(IRHISwapchain& InSwapchain, FSize InSize, const FBuffer& InBuffer, bool bInStorage)
{
	FPassCommands Commands;
	Commands.Name = "Storage transition usage compatibility";
	Commands.Transitions = {{{}, EResourceState::ShaderRead, EResourceState::ShaderWrite, 0, 0, InBuffer}};
	InSwapchain.BeginFrame(InSize);
	bool bRejected = false;
	try
	{
		HYP_CHECK(InSwapchain.Record(0, Commands).Payload);
	}
	catch (const std::invalid_argument& Error)
	{
		HYP_CHECK(std::string(Error.what()) == "Invalid storage buffer transition");
		bRejected = true;
	}
	InSwapchain.CancelFrame();
	HYP_CHECK(bRejected != bInStorage);
}

void CheckStorageOperations(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, FSize InSize)
{
	const std::array Initial{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
	for (const std::uint32_t Usage : {8U, 16U, 24U, 32U, 64U, 96U, 120U})
	{
		const bool bStorage = Usage == 32 || Usage == 64 || Usage == 96 || Usage == 120;
		const auto Buffer = InDevice.CreateBuffer({4, Usage}, Initial);
		CheckStorageReadback(InDevice, Buffer, bStorage);
		CheckStorageTransition(InSwapchain, InSize, Buffer, bStorage);
	}
}
} // namespace

void CheckBufferUsageCreation(IRHIDevice& InDevice)
{
	CheckCreationMatrix(InDevice);
}

void CheckBufferUsageAccess(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, FSize InSize)
{
	const std::array Cases{FAccessCase{8, EResourceState::ShaderRead, false, true},
	                       FAccessCase{16, EResourceState::ShaderRead, true, true},
	                       FAccessCase{32, EResourceState::ShaderRead, true, false},
	                       FAccessCase{64, EResourceState::ShaderRead, false, false},
	                       FAccessCase{96, EResourceState::ShaderRead, true, false},
	                       FAccessCase{32, EResourceState::ShaderWrite, true, true},
	                       FAccessCase{64, EResourceState::ShaderWrite, true, true},
	                       FAccessCase{8, EResourceState::ShaderWrite, true, false},
	                       FAccessCase{16, EResourceState::ShaderWrite, true, false},
	                       FAccessCase{120, EResourceState::ShaderRead, true, true},
	                       FAccessCase{120, EResourceState::ShaderWrite, false, false},
	                       FAccessCase{120, EResourceState::CopySource, true, false}};
	for (const auto& Case : Cases)
	{
		CheckAccess(InDevice, InSwapchain, InSize, Case);
	}
	CheckStorageOperations(InDevice, InSwapchain, InSize);
}
