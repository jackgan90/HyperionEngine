#include "Hyperion/Automation/Endpoint.h"
#include "Hyperion/RenderControls/RenderSettings.h"
#include "Hyperion/RenderControls/SceneViewport.h"
#include "SceneOperations.h"
#include <array>
#include <fstream>
#include <source_location>

namespace
{
using namespace Hyperion;

void Check(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error("Render option contract failed at " + std::to_string(InLocation.line()));
	}
}

const FArchiveNode& Field(const FArchiveNode& InValue, const char* InKey)
{
	return std::get<FArchiveNode::FObject>(InValue.Value).at(InKey);
}

void CheckRenderSchema(const FArchiveNode& InSchema)
{
	Check(ReadValue<std::string>(Field(InSchema, "x-hyperion-type")) == "hyperion.render.settings");
	Check(ReadValue<std::uint32_t>(Field(InSchema, "x-hyperion-version")) == 1);
	Check(std::get<FArchiveNode::FArray>(Field(InSchema, "required").Value).empty());
	const auto& Properties = Field(InSchema, "properties");
	Check(std::get<FArchiveNode::FObject>(Properties.Value).size() == 9);
	for (const auto* Key : {"pipeline", "gbuffer"})
	{
		const auto& Property = Field(Properties, Key);
		Check(ReadValue<std::string>(Field(Property, "type")) == "string");
		Check(!std::get<FArchiveNode::FObject>(Property.Value).contains("enum"));
	}
	Check(ReadValue<std::string>(Field(Field(Properties, "pipeline"), "default")) == "deferred");
	Check(ReadValue<std::string>(Field(Field(Properties, "gbuffer"), "default")) == "compact");
	const auto& Debug = Field(Properties, "debugMode");
	Check(ReadValue<std::string>(Field(Debug, "type")) == "integer");
	Check(ReadValue<double>(Field(Debug, "minimum")) == 0);
	Check(ReadValue<double>(Field(Debug, "maximum")) == 4294967295.0);
	Check(ReadValue<std::uint32_t>(Field(Debug, "default")) == 0);
}

void CheckViewportSchema(const FArchiveNode& InSchema)
{
	Check(ReadValue<std::string>(Field(InSchema, "x-hyperion-type")) == "hyperion.viewport.options");
	Check(ReadValue<std::uint32_t>(Field(InSchema, "x-hyperion-version")) == 1);
	Check(std::get<FArchiveNode::FArray>(Field(InSchema, "required").Value).empty());
	const auto& Properties = Field(InSchema, "properties");
	Check(std::get<FArchiveNode::FObject>(Properties.Value).size() == 14);
	for (const auto* Key : {"visualizer", "culling", "outlineMode"})
	{
		const auto& Property = Field(Properties, Key);
		Check(std::holds_alternative<std::monostate>(Field(Property, "default").Value));
		const auto& Alternatives = std::get<FArchiveNode::FArray>(Field(Property, "anyOf").Value);
		Check(Alternatives.size() == 2);
		Check(ReadValue<std::string>(Field(Alternatives[0], "type")) == "integer");
		Check(ReadValue<double>(Field(Alternatives[0], "minimum")) == 0);
		Check(ReadValue<double>(Field(Alternatives[0], "maximum")) == 4294967295.0);
		Check(!std::get<FArchiveNode::FObject>(Alternatives[0].Value).contains("enum"));
		Check(ReadValue<std::string>(Field(Alternatives[1], "type")) == "null");
	}
}

void CheckDiscovery()
{
	FOperationCatalog Catalog;
	RegisterRenderSettings(Catalog, nullptr);
	RegisterViewportOperations(Catalog, nullptr, nullptr);
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	FAutomationEndpoint Endpoint(Session);
	FArchiveNode::FObject Operations;
	const std::array<std::pair<const char*, std::uint32_t>, 5> Expected{{{"render.settings.get", 2},
	                                                                     {"render.settings.set", 2},
	                                                                     {"render.settings.save", 1},
	                                                                     {"view.get", 1},
	                                                                     {"view.set", 1}}};
	for (const auto& [Id, Version] : Expected)
	{
		const auto Search =
		    Endpoint.Execute("api.search", FArchiveNode(FArchiveNode::FObject{{"query", WriteValue(std::string(Id))}}));
		Check(WriteJson(Search).find(Id) != std::string::npos);
		const auto Description = Endpoint.Execute(
		    "api.describe", FArchiveNode(FArchiveNode::FObject{{"operation", WriteValue(std::string(Id))}}));
		Check(ReadValue<std::uint32_t>(Field(Description, "version")) == Version);
		Check(ReadValue<std::string>(Field(Description, "executionDomain")) == "Main");
		const auto Failure = Endpoint.Execute(
		    "api.call", FArchiveNode(FArchiveNode::FObject{{"operation", WriteValue(std::string(Id))},
		                                                   {"arguments", Catalog.Find(Id).Info.Example}}));
		Check(ReadValue<std::string>(Field(Field(Failure, "error"), "code")) == "unavailable");
		Operations.emplace(Id, Description);
	}
	FArchiveNode::FObject Types;
	for (const auto* Id : {"hyperion.render.settings", "hyperion.viewport.options"})
	{
		Types.emplace(Id, Endpoint.Execute("types.describe",
		                                   FArchiveNode(FArchiveNode::FObject{{"type", WriteValue(std::string(Id))}})));
	}
	CheckRenderSchema(Types.at("hyperion.render.settings"));
	CheckViewportSchema(Types.at("hyperion.viewport.options"));
	const FArchiveNode Snapshot(FArchiveNode::FObject{{"operations", FArchiveNode(std::move(Operations))},
	                                                  {"types", FArchiveNode(std::move(Types))}});
	std::ofstream File("render-option-api-contract.json", std::ios::binary);
	File << WriteJson(Snapshot);
	Check(File.good());
}
} // namespace

void CheckRenderOptionOperations()
{
	CheckDiscovery();
}
