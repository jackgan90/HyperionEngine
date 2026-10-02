#pragma once
#include "Hyperion/RHI/RHIDevice.h"

namespace Hyperion::Tests
{
struct FDrawStateFixture
{
	FPipelineDesc PipelineA;
	FDrawPacket A;
	FDrawPacket B;
	FDrawPacket A2;
	FBuffer Page;
	FBufferSlice WhiteTint;

	explicit FDrawStateFixture(IRHIDevice& InDevice, bool bInFullScreen);
};

FPassCommands MakeDrawStateCommands(std::vector<FDrawPacket> InDraws, bool bInStencil = false);
void CheckNativeDrawStateFixtures(IRHIDevice& InDevice);
} // namespace Hyperion::Tests
