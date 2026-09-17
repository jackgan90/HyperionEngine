#pragma once
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
// Opt-in wall-clock measurement for reproducible host benchmarks. Disabled scopes read no clock.
class FMeasurementScope
{
public:
	FMeasurementScope(bool bInEnabled, double& OutMilliseconds)
	    : Output(bInEnabled ? &OutMilliseconds : nullptr), Started(bInEnabled ? ClockNanoseconds() : 0)
	{
	}

	~FMeasurementScope()
	{
		if (Output)
		{
			*Output += double(ClockNanoseconds() - Started) / 1e6;
		}
	}

	FMeasurementScope(const FMeasurementScope&) = delete;
	FMeasurementScope& operator=(const FMeasurementScope&) = delete;

private:
	double* Output{};
	std::uint64_t Started{};
};
} // namespace Hyperion
