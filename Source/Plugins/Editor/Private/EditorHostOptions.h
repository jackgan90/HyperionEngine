#pragma once

namespace Hyperion
{
struct FEditorOptions;
bool HasEditorAcceptanceRequest(const FEditorOptions& InOptions);
bool ShouldPersistEditorGui(const FEditorOptions& InOptions);
} // namespace Hyperion
