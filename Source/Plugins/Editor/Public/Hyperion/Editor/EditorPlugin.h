#pragma once
#include "Hyperion/ApplicationServices/ApplicationServices.h"

namespace Hyperion
{
class FLogHistory;
class FStorageSettings;
std::shared_ptr<FStorageSettings> CreateEditorStorage(int InCount, char** InValues);
void RunEditorApplication(int InCount, char** InValues, FRegisterBackends InBackends,
                          FLogHistory* InLogHistory = nullptr, std::shared_ptr<FStorageSettings> InStorage = {});
} // namespace Hyperion
