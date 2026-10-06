#include "Hyperion/Config/StorageSettings.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Logging/LogHistory.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Editor/EditorPlugin.h"
#include "Hyperion/Platform/ProcessOutput.h"
#include <iostream>

int main(int InCount, char** InValues)
{
	bool bInteractive = !Hyperion::FProcessOutput::IsRedirected();
	for (int Index = 1; Index < InCount; ++Index)
	{
		const std::string_view Argument(InValues[Index]);
		if (Argument == "--hidden" || Argument == "--frames" || Argument == "--kernel-only" ||
		    Argument == "--benchmark" || Argument.starts_with("--exercise"))
		{
			bInteractive = false;
		}
	}
	std::unique_ptr<Hyperion::FProcessOutput> Output;
	try
	{
		auto Storage = Hyperion::CreateEditorStorage(InCount, InValues);
		const auto Directory = Storage->Paths().Logs / ("Session-" + std::to_string(Hyperion::ClockNanoseconds()));
		auto History = std::make_shared<Hyperion::FLogHistory>(
		    Directory / ("EditorSession-" + std::to_string(Hyperion::ClockNanoseconds()) + ".bin"));
		Hyperion::InitializeEditorLog(Directory / "editor.log", History,
		                              [&Output](std::string_view InText, bool bInError)
		                              {
			                              if (Output)
			                              {
				                              Output->WriteOriginal(InText, bInError);
			                              }
		                              });
		Output = std::make_unique<Hyperion::FProcessOutput>(Hyperion::LogStandardOutput);
		Hyperion::Log(Hyperion::ELogLevel::Info, "Hyperion Editor starting");
		Hyperion::Log(Hyperion::ELogLevel::Debug, "Editor log history initialized before plugin startup");
		Hyperion::RunEditorApplication(InCount, InValues, Hyperion::RegisterD3D12RHIBackend, History.get(),
		                               std::move(Storage));
		Output->Stop();
		Hyperion::ShutdownLog();
		return 0;
	}
	catch (const std::exception& Error)
	{
		const std::string Message = "Hyperion Editor: " + std::string(Error.what());
		bool bReported{};
		try
		{
			Hyperion::Log(Hyperion::ELogLevel::Error, Message);
			bReported = Output != nullptr;
		}
		catch (...)
		{
		}
		if (Output)
		{
			if (!bReported)
			{
				Output->WriteOriginal(Message + "\n", true);
			}
			try
			{
				Output->Stop();
			}
			catch (...)
			{
			}
		}
		else
		{
			std::cerr << Message << '\n';
		}
		Hyperion::ShutdownLog();
		Hyperion::FProcessOutput::ReportFailure(Message, bInteractive);
		return 1;
	}
}
