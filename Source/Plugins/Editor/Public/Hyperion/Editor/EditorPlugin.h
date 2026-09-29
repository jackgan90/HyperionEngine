#pragma once
#include "Hyperion/ApplicationServices/ApplicationServices.h"

namespace Hyperion
{
class FLogHistory;
void RunEditorApplication(int InCount, char** InValues, FRegisterBackends InBackends,
                          FLogHistory* InLogHistory = nullptr);
} // namespace Hyperion
