#include "Hyperion/Platform/FileDialog.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <windows.h>

namespace
{
struct FDialogResponse
{
	int Command{};
	std::atomic<bool> bFound{};
};

BOOL CALLBACK Respond(HWND InWindow, LPARAM InParameter)
{
	auto& Response = *reinterpret_cast<FDialogResponse*>(InParameter);
	DWORD Process{};
	GetWindowThreadProcessId(InWindow, &Process);
	wchar_t Title[128]{};
	GetWindowTextW(InWindow, Title, 128);
	if (Process == GetCurrentProcessId() && std::wstring_view(Title) == L"Open asset root")
	{
		PostMessageW(InWindow, WM_COMMAND, static_cast<WPARAM>(Response.Command), 0);
		Response.bFound = true;
		return FALSE;
	}
	return TRUE;
}

void CheckDialog(Hyperion::FWindow& InWindow, const std::filesystem::path& InRoot, bool bInAccept)
{
	FDialogResponse Response{bInAccept ? IDOK : IDCANCEL};
	std::jthread Answer(
	    [&](std::stop_token InStop)
	    {
		    while (!InStop.stop_requested())
		    {
			    EnumWindows(Respond, reinterpret_cast<LPARAM>(&Response));
			    std::this_thread::sleep_for(std::chrono::milliseconds(100));
		    }
	    });
	const auto Result = Hyperion::SelectFolder(InWindow.Surface(), InRoot);
	Answer.request_stop();
	if (!Response.bFound || Result.has_value() != bInAccept ||
	    (Result && std::filesystem::canonical(*Result) != InRoot))
	{
		throw std::runtime_error("Native folder selection/cancellation mismatch");
	}
}
} // namespace

int main()
{
	try
	{
		Hyperion::FWindow Window("Folder dialog test", {320, 200}, true);
		const auto Directory = std::filesystem::current_path() / L"Folder \u6d4b\u8bd5";
		std::filesystem::create_directories(Directory);
		const auto Root = std::filesystem::canonical(Directory);
		CheckDialog(Window, Root, false);
		CheckDialog(Window, Root, true);
		std::cout << "Native folder dialog cancellation and selection passed\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
