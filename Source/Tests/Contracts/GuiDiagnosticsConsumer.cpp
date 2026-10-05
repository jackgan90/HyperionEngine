#include "Hyperion/GuiRenderer/Diagnostics.h"

int main()
{
	const Hyperion::FDebugMetrics Metrics;
	return Metrics.bContactShadows ? 1 : 0;
}
