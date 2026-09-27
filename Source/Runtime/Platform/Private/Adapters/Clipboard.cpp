#include "Clipboard.h"
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <Windows.h>

namespace Hyperion
{
namespace
{
constexpr std::size_t MaxClipboardBytes = 1024 * 1024;

std::wstring Wide(const std::string& InValue)
{
	if (InValue.empty())
	{
		return {};
	}
	if (InValue.size() > MaxClipboardBytes || InValue.find('\0') != std::string::npos)
	{
		throw std::invalid_argument("Clipboard text is too large or contains a null character");
	}
	const auto Size =
	    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, InValue.data(), int(InValue.size()), nullptr, 0);
	if (!Size)
	{
		throw std::invalid_argument("Clipboard text is not valid UTF-8");
	}
	std::wstring Result(Size, L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, InValue.data(), int(InValue.size()), Result.data(), Size);
	return Result;
}

UINT Format(const std::string& InFormat)
{
	if (InFormat.empty() || InFormat.size() > 255)
	{
		throw std::invalid_argument("Invalid clipboard format");
	}
	const auto Result = RegisterClipboardFormatW(Wide(InFormat).c_str());
	if (!Result)
	{
		throw std::runtime_error("Cannot register clipboard format");
	}
	return Result;
}

struct FClipboardAccess
{
	explicit FClipboardAccess(FNativeSurface InSurface)
	{
		if (!OpenClipboard(static_cast<HWND>(InSurface.Handle)))
		{
			throw std::runtime_error("System clipboard is busy or inaccessible; try again");
		}
	}

	~FClipboardAccess()
	{
		CloseClipboard();
	}
};

struct FClipboardMemory
{
	HGLOBAL Handle{};

	FClipboardMemory(const void* InData, std::size_t InBytes)
	{
		Handle = GlobalAlloc(GMEM_MOVEABLE, InBytes);
		const auto Memory = Handle ? GlobalLock(Handle) : nullptr;
		if (!Memory)
		{
			if (Handle)
			{
				GlobalFree(Handle);
			}
			throw std::runtime_error("Cannot allocate clipboard data");
		}
		std::memcpy(Memory, InData, InBytes);
		GlobalUnlock(Handle);
	}

	~FClipboardMemory()
	{
		if (Handle)
		{
			GlobalFree(Handle);
		}
	}

	void Publish(UINT InFormat)
	{
		if (!SetClipboardData(InFormat, Handle))
		{
			throw std::runtime_error("Cannot write system clipboard data");
		}
		Handle = nullptr;
	}
};
} // namespace

std::string ReadTypedClipboard(FNativeSurface InSurface, const std::string& InFormat)
{
	const auto Type = Format(InFormat);
	FClipboardAccess Access(InSurface);
	if (!IsClipboardFormatAvailable(Type))
	{
		return {};
	}
	const auto Memory = GetClipboardData(Type);
	const auto Size = Memory ? GlobalSize(Memory) : 0;
	if (!Size || Size > MaxClipboardBytes)
	{
		throw std::runtime_error("Invalid or oversized system clipboard data");
	}
	const auto Data = static_cast<const char*>(GlobalLock(Memory));
	if (!Data)
	{
		throw std::runtime_error("Cannot read system clipboard data");
	}
	try
	{
		const auto End = static_cast<const char*>(std::memchr(Data, 0, Size));
		if (!End)
		{
			throw std::runtime_error("Malformed system clipboard string");
		}
		std::string Result(Data, End);
		GlobalUnlock(Memory);
		return Result;
	}
	catch (...)
	{
		GlobalUnlock(Memory);
		throw;
	}
}

void WriteTypedClipboard(FNativeSurface InSurface, const std::string& InFormat, const std::string& InData,
                         const std::string& InText)
{
	if (InData.empty() || InData.size() >= MaxClipboardBytes || InData.find('\0') != std::string::npos)
	{
		throw std::invalid_argument("Invalid typed clipboard string");
	}
	const auto Type = Format(InFormat);
	const auto Text = Wide(InText);
	FClipboardMemory Payload(InData.c_str(), InData.size() + 1);
	FClipboardMemory PlainText(Text.c_str(), (Text.size() + 1) * sizeof(wchar_t));
	FClipboardAccess Access(InSurface);
	if (!EmptyClipboard())
	{
		throw std::runtime_error("Cannot replace system clipboard");
	}
	PlainText.Publish(CF_UNICODETEXT);
	Payload.Publish(Type);
}
} // namespace Hyperion
#else
namespace Hyperion
{
std::string ReadTypedClipboard(FNativeSurface, const std::string&)
{
	throw std::runtime_error("Typed system clipboard is unavailable on this platform");
}

void WriteTypedClipboard(FNativeSurface, const std::string&, const std::string&, const std::string&)
{
	throw std::runtime_error("Typed system clipboard is unavailable on this platform");
}
} // namespace Hyperion
#endif
