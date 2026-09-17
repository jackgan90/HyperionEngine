#pragma once

namespace Hyperion::Tests
{
// Test-executable-only ordinary-new failure at a Dispatch shared_ptr control-block allocation.
class FDispatchAllocationFailure
{
public:
	explicit FDispatchAllocationFailure(int InIndex);
	FDispatchAllocationFailure(int InIndex, const char* InFunction, const char* InAllocationSymbol = nullptr);
	~FDispatchAllocationFailure();
	FDispatchAllocationFailure(const FDispatchAllocationFailure&) = delete;
	FDispatchAllocationFailure& operator=(const FDispatchAllocationFailure&) = delete;
	bool WasInjected() const;
};
} // namespace Hyperion::Tests
