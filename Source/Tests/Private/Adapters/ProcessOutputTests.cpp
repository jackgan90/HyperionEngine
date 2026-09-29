#include "Hyperion/Core/Logging/LogHistory.h"
#include "Hyperion/Platform/ProcessOutput.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <thread>
#include <windows.h>

namespace Hyperion
{
namespace
{
void CheckUtf8Capture(const std::filesystem::path& InDirectory, std::string_view InCharacter, std::size_t InPrefixBytes,
                      bool bInNewline)
{
	FLogHistory History(InDirectory / "Utf8Capture.bin");
	FProcessOutput Output(
	    [&History](bool bInError, std::string_view InText)
	    {
		    History.Append(bInError ? ELogLevel::Error : ELogLevel::Info, InText, "stdout");
	    });
	const std::string Expected = std::string(8192 - InPrefixBytes, 'u') + std::string(InCharacter);
	DWORD Written{};
	HYP_CHECK(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), Expected.data(), 8192, &Written, nullptr));
	HYP_CHECK(Written == 8192);
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!History.Count() && std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	// Force the reader to process the incomplete character before supplying its continuation.
	HYP_CHECK(History.Count() > 0);
	const std::string Suffix = Expected.substr(8192) + (bInNewline ? "\n" : "");
	HYP_CHECK(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), Suffix.data(), static_cast<DWORD>(Suffix.size()), &Written,
	                    nullptr));
	HYP_CHECK(Written == Suffix.size());
	Output.Stop();
	std::string Recovered;
	for (const auto& Entry : History.Read({0, 256}).Entries)
	{
		HYP_CHECK(!Entry.Message.empty());
		HYP_CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Entry.Message.data(),
		                              static_cast<int>(Entry.Message.size()), nullptr, 0) > 0);
		Recovered += Entry.Message;
	}
	HYP_CHECK(Recovered == Expected);
}

void CheckUtf8Boundaries(const std::filesystem::path& InDirectory)
{
	for (const std::string_view Character : {"\xc2\xa2", "\xe4\xb8\xad", "\xf0\x9f\x98\x80"})
	{
		for (std::size_t PrefixBytes = 1; PrefixBytes < Character.size(); ++PrefixBytes)
		{
			CheckUtf8Capture(InDirectory, Character, PrefixBytes, true);
			CheckUtf8Capture(InDirectory, Character, PrefixBytes, false);
		}
	}
}
} // namespace
} // namespace Hyperion

int main(int InCount, char** InValues)
{
	using namespace Hyperion;
	try
	{
		HYP_CHECK(InCount == 2);
		const std::filesystem::path Directory(InValues[1]);
		CheckUtf8Boundaries(Directory);
		auto History = std::make_shared<FLogHistory>(Directory / "Capture.bin");
		FProcessOutput Output(LogStandardOutput);
		DWORD HandleFlags{};
		HYP_CHECK(GetHandleInformation(GetStdHandle(STD_OUTPUT_HANDLE), &HandleFlags));
		HYP_CHECK(!(HandleFlags & HANDLE_FLAG_INHERIT));
		InitializeEditorLog(Directory / "Capture.log", History,
		                    [&Output](std::string_view InText, bool bInError)
		                    {
			                    Output.WriteOriginal(InText, bInError);
		                    });
		Log(ELogLevel::Debug, "structured-debug");
		Log(ELogLevel::Warning, "structured-warning");
		std::cout << "stdout-";
		std::cout << "joined\n";
		fprintf(stderr, "stderr-crt\n");
		DWORD Written{};
		const std::string Native = "stderr-native\n";
		HYP_CHECK(WriteFile(GetStdHandle(STD_ERROR_HANDLE), Native.data(), static_cast<DWORD>(Native.size()), &Written,
		                    nullptr));
		const std::string Long(24000, 'x');
		std::cout << Long << '\n';
		std::cout << "unterminated-tail";
		Output.Stop();
		ShutdownLog();
		std::string Captured;
		std::size_t DebugCount{};
		std::size_t ErrorCount{};
		std::size_t LongBytes{};
		std::uint64_t Cursor{};
		while (Cursor < History->Count())
		{
			const auto Page = History->Read({Cursor, 256});
			for (const auto& Entry : Page.Entries)
			{
				Captured += Entry.Message + "\n";
				DebugCount += Entry.Level == ELogLevel::Debug;
				ErrorCount += Entry.Level == ELogLevel::Error && Entry.Source == "stderr";
				if (!Entry.Message.empty() && Entry.Message.front() == 'x')
				{
					LongBytes += Entry.Message.size();
				}
			}
			Cursor = Page.Next;
		}
		HYP_CHECK(DebugCount == 1 && ErrorCount == 2 && LongBytes == Long.size());
		HYP_CHECK(Captured.find("stdout-joined") != std::string::npos);
		HYP_CHECK(Captured.find("unterminated-tail") != std::string::npos);
		std::ofstream(Directory / "History.txt", std::ios::binary) << Captured;
		std::cout << "\ncapture-passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
