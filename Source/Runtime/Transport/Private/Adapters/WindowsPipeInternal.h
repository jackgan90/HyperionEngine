#pragma once
#include "Hyperion/Transport/Transport.h"
#include <Windows.h>
#include <utility>

namespace Hyperion
{
class FPipeHandle
{
public:
	explicit FPipeHandle(HANDLE InValue = INVALID_HANDLE_VALUE) : Value(InValue)
	{
	}

	~FPipeHandle()
	{
		if (Value && Value != INVALID_HANDLE_VALUE)
		{
			CloseHandle(Value);
		}
	}

	FPipeHandle(FPipeHandle&& InOther) noexcept : Value(std::exchange(InOther.Value, INVALID_HANDLE_VALUE))
	{
	}

	FPipeHandle(const FPipeHandle&) = delete;
	FPipeHandle& operator=(const FPipeHandle&) = delete;
	HANDLE Value;
};

class FPipeSecurity
{
public:
	FPipeSecurity();
	~FPipeSecurity();
	FPipeSecurity(const FPipeSecurity&) = delete;
	FPipeSecurity& operator=(const FPipeSecurity&) = delete;
	SECURITY_ATTRIBUTES Attributes{};
};

std::wstring CurrentPipeUser();
void VerifyPipeServerUser(HANDLE InPipe);
std::wstring NativePipeName(const std::string& InAddress);
} // namespace Hyperion
