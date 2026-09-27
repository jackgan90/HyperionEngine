#pragma once
#include "Hyperion/Scene/SceneNode.h"

namespace Hyperion
{
constexpr std::size_t SceneClipboardMaxNodes = 16384;
constexpr std::size_t SceneClipboardMaxBytes = 64 * 1024 * 1024;
FSceneNode CloneSceneClipboardNode(const FSceneNode& InNode, std::size_t& OutBytes);
void VisitSceneClipboardReferences(FSceneNode& InNode, const std::function<void(std::string&)>& InVisitor);
// Registers value-copy contracts for built-ins, with material freezing and explicit node references.
void ConfigureSceneClipboardComponent(FSceneComponentDescriptor& InDescriptor);
} // namespace Hyperion
