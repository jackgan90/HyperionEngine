#include "Support/DispatchAllocationFailure.h"
#include "Support/TestSupport.h"
#include <array>
#include <crtdbg.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <new>
#include <windows.h>

#include <dbghelp.h>

namespace Hyperion::Tests
{
namespace
{
thread_local int FailureIndex = -1;
thread_local bool bInjected{};
HANDLE SymbolProcess{};
std::uintptr_t DispatchStart{};
std::uintptr_t DispatchEnd{};
const char* AllocationSymbol{};
bool bCustomFunction{};

void InjectAllocationFailure()
{
	if (FailureIndex < 0)
	{
		return;
	}
	// DbgHelp may allocate internally. Disable the thread-local injector during stack inspection.
	const int Remaining = FailureIndex;
	FailureIndex = -1;
	std::array<void*, 32> Frames{};
	const auto Count = CaptureStackBackTrace(0, static_cast<DWORD>(Frames.size()), Frames.data(), nullptr);
	bool bControlBlock = bCustomFunction && !AllocationSymbol;
	alignas(SYMBOL_INFO) std::array<char, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> Storage{};
	auto* Symbol = reinterpret_cast<SYMBOL_INFO*>(Storage.data());
	Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	Symbol->MaxNameLen = MAX_SYM_NAME;
	for (unsigned Index = 0; Index < Count; ++Index)
	{
		if (AllocationSymbol && SymFromAddr(SymbolProcess, reinterpret_cast<DWORD64>(Frames[Index]), nullptr, Symbol) &&
		    std::strstr(Symbol->Name, AllocationSymbol) && std::strstr(Symbol->Name, "::emplace<"))
		{
			bControlBlock = true;
		}
		if (SymFromAddr(SymbolProcess, reinterpret_cast<DWORD64>(Frames[Index]), nullptr, Symbol) &&
		    std::strstr(Symbol->Name, "_Setpd") &&
		    (std::strstr(Symbol->Name, "FTaskSystem::FImpl::FWork") || std::strstr(Symbol->Name, "FTaskState")))
		{
			bControlBlock = true;
		}
	}
	FailureIndex = Remaining;
	// Debug iterator-proxy allocation inside noexcept container constructors is not a recoverable site.
	if (!bControlBlock)
	{
		return;
	}
	for (unsigned Index = 0; Index < Count; ++Index)
	{
		const auto Address = reinterpret_cast<std::uintptr_t>(Frames[Index]);
		if (Address >= DispatchStart && Address < DispatchEnd)
		{
			if (FailureIndex-- == 0)
			{
				bInjected = true;
				throw std::bad_alloc();
			}
			return;
		}
	}
}

} // namespace

FDispatchAllocationFailure::FDispatchAllocationFailure(int InIndex) : FDispatchAllocationFailure(InIndex, nullptr)
{
}

FDispatchAllocationFailure::FDispatchAllocationFailure(int InIndex, const char* InFunction,
                                                       const char* InAllocationSymbol)
{
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	HYP_CHECK(!SymbolProcess && InIndex >= 0);
	const auto Process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
	HYP_CHECK(Process);
	const auto SearchPath = std::filesystem::absolute(".").string();
	HYP_CHECK(SymInitialize(Process, SearchPath.c_str(), TRUE));
	alignas(SYMBOL_INFO) std::array<char, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> Storage{};
	auto* Symbol = reinterpret_cast<SYMBOL_INFO*>(Storage.data());
	Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	Symbol->MaxNameLen = MAX_SYM_NAME;
	HYP_CHECK(SymFromName(Process, InFunction ? InFunction : "Hyperion::FTaskSystem::Dispatch", Symbol));
	DispatchStart = Symbol->Address;
	DispatchEnd = DispatchStart + Symbol->Size;
	HYP_CHECK(DispatchEnd > DispatchStart);
	SymbolProcess = Process;
	bInjected = false;
	bCustomFunction = InFunction != nullptr;
	AllocationSymbol = InAllocationSymbol;
	FailureIndex = InIndex;
}

FDispatchAllocationFailure::~FDispatchAllocationFailure()
{
	FailureIndex = -1;
	SymCleanup(SymbolProcess);
	CloseHandle(SymbolProcess);
	SymbolProcess = nullptr;
}

bool FDispatchAllocationFailure::WasInjected() const
{
	return bInjected;
}
} // namespace Hyperion::Tests

// Only this test executable replaces ordinary new. Engine allocations and other threads are unaffected.
void* operator new(std::size_t InSize)
{
	Hyperion::Tests::InjectAllocationFailure();
	if (auto* Result = std::malloc(InSize ? InSize : 1))
	{
		return Result;
	}
	throw std::bad_alloc();
}

void* operator new[](std::size_t InSize)
{
	return ::operator new(InSize);
}

void operator delete(void* InPointer) noexcept
{
	std::free(InPointer);
}

void operator delete[](void* InPointer) noexcept
{
	std::free(InPointer);
}

void operator delete(void* InPointer, std::size_t) noexcept
{
	std::free(InPointer);
}

void operator delete[](void* InPointer, std::size_t) noexcept
{
	std::free(InPointer);
}
