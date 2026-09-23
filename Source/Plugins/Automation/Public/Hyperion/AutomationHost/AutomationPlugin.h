#pragma once
#include "Hyperion/Automation/Endpoint.h"
#include "Hyperion/Plugins/PluginRuntime.h"

namespace Hyperion
{
enum class EAutomationTransport
{
	JsonLines,
	Mcp,
	Once
};

struct FAutomationStreamOptions
{
	EAutomationTransport Transport = EAutomationTransport::JsonLines;
	std::string Method;
	std::string Attach;
	FArchiveNode Parameters = FArchiveNode(FArchiveNode::FObject{});
};

struct FAutomationStreamStatus
{
	bool bFailed{};
};

// Adapters require FOperationCatalog and declare Before={"automation-session"}.
// Their state must outlive session Quiesce, which drains every admitted job.
void RegisterAutomationServices(FPluginRegistry& InRegistry);
void RegisterAutomationLocal(FPluginRegistry& InRegistry, std::string InApplication);
void RegisterAssetAutomation(FPluginRegistry& InRegistry);
void RegisterSceneAutomation(FPluginRegistry& InRegistry);
void RegisterAutomationStdio(FPluginRegistry& InRegistry, FAutomationStreamOptions InOptions);
} // namespace Hyperion
