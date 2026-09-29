#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Core/Logging/LogHistory.h"
#include "Support/TestSupport.h"
#include <iostream>

namespace
{
using namespace Hyperion;

class FHistoryProvider final : public FPlugin
{
public:
	explicit FHistoryProvider(FLogHistory& InHistory) : History(InHistory)
	{
	}

	void Start(FPluginContext& InContext) override
	{
		InContext.Provide(History);
	}

private:
	FLogHistory& History;
};

void Check(bool bInProvider)
{
	FLogHistory History(std::filesystem::temp_directory_path() /
	                    ("HyperionLogAutomation-" + std::to_string(ClockNanoseconds()) + ".bin"));
	History.Append(ELogLevel::Debug, "early-debug");
	History.Append(ELogLevel::Warning, "warning");
	FApplicationHost Host(1, 1);
	FPluginRegistry Registry;
	RegisterAutomationServices(Registry);
	RegisterLogAutomation(Registry);
	FPluginSelection Selection;
	Selection.Requested = {"automation-log", "automation-session"};
	if (bInProvider)
	{
		FPluginDescriptor Provider;
		Provider.Id = "log-test-provider";
		Provider.Provides = {typeid(FLogHistory)};
		Provider.Create = [&History]
		{
			return std::make_unique<FHistoryProvider>(History);
		};
		Registry.Add(std::move(Provider));
		Selection.Requested.push_back("log-test-provider");
	}
	Host.Start(Registry, Selection);
	auto& Endpoint = Host.GetServices().Require<FAutomationEndpoint>();
	const auto Execute = [&Endpoint](const char* InMethod, const char* InJson)
	{
		return WriteAutomationResponse(Endpoint.Execute(InMethod, ParseJson(InJson)));
	};
	HYP_CHECK(Execute("api.search", R"({"query":"log"})").find("application.log.read") != std::string::npos);
	const auto Schema = Execute("api.describe", R"({"operation":"application.log.read"})");
	HYP_CHECK(Schema.find("debug") != std::string::npos && Schema.find("after") != std::string::npos);
	const auto First = Execute("api.call", R"({"operation":"application.log.read","arguments":{"limit":1}})");
	if (bInProvider)
	{
		HYP_CHECK(First.find("early-debug") != std::string::npos && First.find("\"level\":3") != std::string::npos);
		const auto Next = Execute("api.call", R"({"operation":"application.log.read","arguments":{"after":"1"}})");
		HYP_CHECK(Next.find("warning") != std::string::npos && Next.find("early-debug") == std::string::npos);
		const auto Invalid = Execute("api.call", R"({"operation":"application.log.read","arguments":{"limit":0}})");
		HYP_CHECK(Invalid.find("invalid_arguments") != std::string::npos && History.Count() == 2);
	}
	else
	{
		HYP_CHECK(First.find("unavailable") != std::string::npos);
	}
	Host.Stop();
	Host.GetServices().Require<FApplicationControl>().RethrowFailure();
}
} // namespace

int main()
{
	try
	{
		Check(true);
		Check(false);
		std::cout << "Log automation discovery, reflected schema, reads, validation and absence passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
