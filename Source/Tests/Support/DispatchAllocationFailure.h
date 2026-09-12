#pragma once

namespace Hyperion::Tests
{
// Test-executable-only ordinary-new failure at a Dispatch shared_ptr control-block allocation.
class FDispatchAllocationFailure
{
public:
	explicit FDispatchAllocationFailure(int InIndex);
	~FDispatchAllocationFailure();
	FDispatchAllocationFailure(const FDispatchAllocationFailure&) = delete;
	FDispatchAllocationFailure& operator=(const FDispatchAllocationFailure&) = delete;
	bool WasInjected() const;
};
} // namespace Hyperion::Tests
