#include "Hyperion/Platform/Window.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <thread>
#include <windows.h>

using namespace Hyperion;

int main()
{
	try
	{
		FWindow Window("Clipboard test", {100, 100}, true);
		const std::string Format = "Hyperion.ClipboardTest.v1";
		HYP_CHECK(Window.SupportsTypedClipboard());
		Window.SetTypedClipboard(Format, "snapshot-token", "Object names");
		HYP_CHECK(Window.TypedClipboard(Format) == "snapshot-token");
		HYP_CHECK(Window.Clipboard() == "Object names");
		Window.SetClipboard("ordinary text");
		HYP_CHECK(Window.TypedClipboard(Format).empty());
		Window.SetTypedClipboard(Format, "new-token", "another name");
		// Simulate a foreign application, bypassing SDL and the engine's clipboard bookkeeping.
		HYP_CHECK(OpenClipboard(static_cast<HWND>(Window.Surface().Handle)));
		HYP_CHECK(EmptyClipboard());
		const auto Memory = GlobalAlloc(GMEM_MOVEABLE, 4 * sizeof(wchar_t));
		const auto Text = static_cast<wchar_t*>(GlobalLock(Memory));
		HYP_CHECK(Text);
		Text[0] = L'a';
		Text[1] = L'b';
		Text[2] = L'c';
		Text[3] = 0;
		GlobalUnlock(Memory);
		HYP_CHECK(SetClipboardData(CF_UNICODETEXT, Memory));
		CloseClipboard();
		HYP_CHECK(Window.TypedClipboard(Format).empty() && Window.Clipboard() == "abc");
		Window.SetTypedClipboard(Format, "third-token", "names");
		HYP_CHECK(OpenClipboard(static_cast<HWND>(Window.Surface().Handle)));
		HYP_CHECK(EmptyClipboard());
		CloseClipboard();
		HYP_CHECK(Window.TypedClipboard(Format).empty());
		bool bRejected{};
		std::jthread WrongThread(
		    [&]
		    {
			    try
			    {
				    Window.TypedClipboard(Format);
			    }
			    catch (const std::logic_error&)
			    {
				    bRejected = true;
			    }
		    });
		WrongThread.join();
		HYP_CHECK(bRejected);
		std::cout << "Typed system clipboard, text replacement, external ownership and thread checks passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
