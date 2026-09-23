#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/IO/Path.h"
#include <fstream>
#include <iostream>

namespace Hyperion
{
namespace
{
struct FOptions
{
	FAutomationStreamOptions Stream;
	FAssetServiceOptions Assets;
	FPluginSelection Selection{{"assets", "automation-assets", "automation-stdio"}};
};

FOptions ParseOptions(int InCount, char** InValues)
{
	FOptions Options;
	const auto Root = std::filesystem::path(HYP_SOURCE_DIR);
	Options.Assets.EngineContent = Root / "Content";
	bool bMode{};
	bool bParameters{};
	bool bStandaloneOptions{};
	bool bAttach{};
	for (int Index = 1; Index < InCount; ++Index)
	{
		const std::string Argument = InValues[Index];
		auto Value = [&]() -> std::string
		{
			if (++Index >= InCount)
			{
				throw std::invalid_argument("Missing value for " + Argument);
			}
			return InValues[Index];
		};
		if (Argument == "--attach")
		{
			if (bAttach)
			{
				throw std::invalid_argument("Specify --attach once");
			}
			bAttach = true;
			Options.Stream.Attach = Value();
			if (Options.Stream.Attach.empty())
			{
				throw std::invalid_argument("--attach requires a non-empty target instance");
			}
		}
		else if (Argument == "--asset-root")
		{
			bStandaloneOptions = true;
			Options.Assets.AssetRoot = PathFromUtf8(Value());
		}
		else if (Argument == "--engine-content")
		{
			bStandaloneOptions = true;
			Options.Assets.EngineContent = PathFromUtf8(Value());
		}
		else if (Argument == "--read-only")
		{
			bStandaloneOptions = true;
			Options.Assets.bReadOnly = true;
		}
		else if (Argument == "--disable-plugin")
		{
			Options.Selection.Disabled.push_back(Value());
		}
		else if (Argument == "--json" || Argument == "--json-file")
		{
			if (bParameters)
			{
				throw std::invalid_argument("Specify parameters once");
			}
			bParameters = true;
			auto Json = Value();
			if (Argument == "--json-file")
			{
				std::ifstream File(PathFromUtf8(Json), std::ios::binary);
				if (!File)
				{
					throw std::invalid_argument("Could not open JSON parameter file");
				}
				Json.resize(FJsonLimits{}.MaxBytes + 1);
				File.read(Json.data(), static_cast<std::streamsize>(Json.size()));
				Json.resize(static_cast<std::size_t>(File.gcount()));
			}
			Options.Stream.Parameters = ParseJson(Json);
		}
		else
		{
			if (bMode)
			{
				throw std::invalid_argument("Specify one command or session mode");
			}
			bMode = true;
			if (Argument == "--stdio")
			{
				Options.Stream.Transport = EAutomationTransport::JsonLines;
			}
			else if (Argument == "--mcp")
			{
				Options.Stream.Transport = EAutomationTransport::Mcp;
			}
			else if (!Argument.starts_with("--"))
			{
				Options.Stream.Transport = EAutomationTransport::Once;
				Options.Stream.Method = Argument;
			}
			else
			{
				throw std::invalid_argument("Unknown option: " + Argument);
			}
		}
	}
	if (!bMode || (bParameters && Options.Stream.Transport != EAutomationTransport::Once))
	{
		throw std::invalid_argument("Specify a command, --stdio, or --mcp; --json/--json-file are one-shot parameters");
	}
	if (bAttach && bStandaloneOptions)
	{
		throw std::invalid_argument(
		    "Asset options configure standalone state; an attached target owns its content settings");
	}
	if (bAttach)
	{
		Options.Selection.Requested = {"automation-stdio"};
	}
	return Options;
}

int Run(int InCount, char** InValues)
{
	const auto Options = ParseOptions(InCount, InValues);
	FApplicationHost Host(4, 1);
	FPluginRegistry Registry;
	RegisterAssetServices(Registry, Options.Assets);
	RegisterAutomationServices(Registry);
	RegisterAssetAutomation(Registry);
	RegisterAutomationStdio(Registry, Options.Stream);
	Host.Start(Registry, Options.Selection);
	if (!Host.GetPlugins().IsActive("automation-stdio"))
	{
		throw std::runtime_error("Automation transport is unavailable; see plugin diagnostics on stderr");
	}
	Host.Run();
	const bool bFailed = Host.GetServices().Require<FAutomationStreamStatus>().bFailed;
	Host.Stop();
	Host.GetServices().Require<FApplicationControl>().RethrowFailure();
	return bFailed ? 1 : 0;
}
} // namespace
} // namespace Hyperion

int main(int InCount, char** InValues)
{
	if (InCount == 2 && std::string_view(InValues[1]) == "--help")
	{
		std::cout
		    << "Hyperion automation\n"
		       "  hyperion_automation_cli <method> [--json <object> | --json-file <file>]\n"
		       "  hyperion_automation_cli --stdio | --mcp\n"
		       "Options: --attach <instance-id>, --asset-root <directory>, --engine-content <directory>, --read-only, "
		       "--disable-plugin <id>\n"
		       "Methods: targets.list, targets.connect, targets.disconnect, engine.info, api.search, api.describe, "
		       "types.describe, api.call, jobs.get, jobs.cancel\n"
		       "JSONL: {\"id\":\"1\",\"method\":\"api.search\",\"params\":{\"query\":\"texture\"}}\n"
		       "Use a persistent session for document workflows; one-shot calls wait for their job before exiting.\n";
		return 0;
	}
	try
	{
		Hyperion::InitializeStderrLog();
		const int Result = Hyperion::Run(InCount, InValues);
		Hyperion::ShutdownLog();
		return Result;
	}
	catch (const std::exception& Error)
	{
		std::cerr << "Hyperion automation: " << Error.what() << '\n';
		Hyperion::ShutdownLog();
		return 1;
	}
}
