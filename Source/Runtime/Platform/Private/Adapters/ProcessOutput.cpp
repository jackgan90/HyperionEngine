#include "Hyperion/Platform/ProcessOutput.h"
#include <algorithm>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <windows.h>

namespace Hyperion
{
namespace
{
bool Valid(HANDLE InHandle)
{
	return InHandle && InHandle != INVALID_HANDLE_VALUE;
}

std::size_t CompletePrefixSize(std::string_view InText, std::size_t InLimit)
{
	const std::size_t Size = std::min(InText.size(), InLimit);
	if (!Size)
	{
		return 0;
	}
	std::size_t Start = Size - 1;
	while (Start && (static_cast<unsigned char>(InText[Start]) & 0xc0) == 0x80)
	{
		--Start;
	}
	const auto Lead = static_cast<unsigned char>(InText[Start]);
	const std::size_t Width = Lead >= 0xc2 && Lead <= 0xdf   ? 2
	                          : Lead >= 0xe0 && Lead <= 0xef ? 3
	                          : Lead >= 0xf0 && Lead <= 0xf4 ? 4
	                                                         : 1;
	// Inspect the final character even when the limit coincides with the current pipe read.
	return Width > Size - Start ? Start : Size;
}

struct FOutputChannel
{
	FILE* Stream{};
	DWORD Kind{};
	HANDLE Original{};
	HANDLE Reader{};
	int Saved = -1;
	bool bRedirected{};
	bool bError{};
	std::thread Worker;
	std::mutex WriteMutex;
	std::string Failure;

	~FOutputChannel()
	{
		Stop();
		if (Valid(Original))
		{
			CloseHandle(Original);
		}
	}

	void Write(std::string_view InText) noexcept
	{
		std::lock_guard Lock(WriteMutex);
		while (Valid(Original) && !InText.empty())
		{
			DWORD Written{};
			if (!WriteFile(Original, InText.data(), static_cast<DWORD>(std::min<std::size_t>(InText.size(), 65536)),
			               &Written, nullptr) ||
			    !Written)
			{
				break;
			}
			InText.remove_prefix(Written);
		}
	}

	void Drain(const FProcessOutput::FReceive& InReceive) noexcept
	{
		std::string Pending;
		char Buffer[4096];
		DWORD Count{};
		try
		{
			while (ReadFile(Reader, Buffer, sizeof(Buffer), &Count, nullptr) && Count)
			{
				Write({Buffer, Count});
				Pending.append(Buffer, Count);
				for (;;)
				{
					const auto End = Pending.find('\n');
					if (End == std::string::npos && Pending.size() < 8192)
					{
						break;
					}
					const std::size_t Size = CompletePrefixSize(
					    Pending, End == std::string::npos ? 8192 : std::min<std::size_t>(End + 1, 8192));
					if (!Size)
					{
						break;
					}
					InReceive(bError, std::string_view(Pending).substr(0, Size));
					Pending.erase(0, Size);
				}
			}
			if (!Pending.empty())
			{
				InReceive(bError, Pending);
			}
		}
		catch (const std::exception& Error)
		{
			Failure = Error.what();
			// Keep draining so a capture failure cannot block producers on a full pipe.
			while (ReadFile(Reader, Buffer, sizeof(Buffer), &Count, nullptr) && Count)
			{
				Write({Buffer, Count});
			}
		}
	}

	void Start(FILE* InStream, DWORD InKind, bool bInError, FProcessOutput::FReceive InReceive)
	{
		Stream = InStream;
		Kind = InKind;
		bError = bInError;
		const auto Existing = GetStdHandle(Kind);
		if (Valid(Existing) && !DuplicateHandle(GetCurrentProcess(), Existing, GetCurrentProcess(), &Original, 0, FALSE,
		                                        DUPLICATE_SAME_ACCESS))
		{
			throw std::runtime_error("Could not preserve standard output");
		}
		if (_fileno(Stream) >= 0 && _get_osfhandle(_fileno(Stream)) != -1)
		{
			fflush(Stream);
			Saved = _dup(_fileno(Stream));
			if (Saved < 0)
			{
				throw std::runtime_error("Could not preserve CRT output");
			}
		}
		else
		{
			FILE* Reopened{};
			if (freopen_s(&Reopened, "NUL", "w", Stream))
			{
				throw std::runtime_error("Could not initialize CRT output");
			}
		}
		HANDLE Writer{};
		if (!CreatePipe(&Reader, &Writer, nullptr, 65536))
		{
			throw std::runtime_error("Could not create output capture pipe");
		}
		const int Descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(Writer), _O_WRONLY | _O_TEXT);
		if (Descriptor < 0)
		{
			CloseHandle(Writer);
			throw std::runtime_error("Could not bind output capture pipe");
		}
		const int Result = _dup2(Descriptor, _fileno(Stream));
		_close(Descriptor);
		if (Result)
		{
			throw std::runtime_error("Could not redirect CRT output");
		}
		bRedirected = true;
		setvbuf(Stream, nullptr, _IONBF, 0);
		const auto Captured = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(Stream)));
		// CRT duplication can make handles inheritable. A child must not keep our pipe alive at Stop.
		if (!SetHandleInformation(Captured, HANDLE_FLAG_INHERIT, 0) || !SetStdHandle(Kind, Captured))
		{
			throw std::runtime_error("Could not redirect native output");
		}
		Worker = std::thread(
		    [this, Receive = std::move(InReceive)]
		    {
			    Drain(Receive);
		    });
	}

	void Stop() noexcept
	{
		bool bRestoreFailed{};
		if (bRedirected)
		{
			fflush(Stream);
			if (Saved >= 0)
			{
				bRestoreFailed = _dup2(Saved, _fileno(Stream)) != 0;
				if (bRestoreFailed)
				{
					_close(_fileno(Stream));
				}
			}
			else
			{
				FILE* Reopened{};
				bRestoreFailed = freopen_s(&Reopened, "NUL", "w", Stream) != 0;
			}
			SetStdHandle(Kind, bRestoreFailed ? nullptr : reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(Stream))));
			bRedirected = false;
		}
		if (Saved >= 0)
		{
			_close(Saved);
			Saved = -1;
		}
		if (Worker.joinable())
		{
			Worker.join();
		}
		if (bRestoreFailed && Failure.empty())
		{
			Failure = "Could not restore standard output";
		}
		if (Valid(Reader))
		{
			CloseHandle(Reader);
			Reader = nullptr;
		}
	}
};
} // namespace

struct FProcessOutput::FImpl
{
	FOutputChannel Out;
	FOutputChannel Error;
};

FProcessOutput::FProcessOutput(FReceive InReceive) : Impl(std::make_unique<FImpl>())
{
	Impl->Out.Start(stdout, STD_OUTPUT_HANDLE, false, InReceive);
	Impl->Error.Start(stderr, STD_ERROR_HANDLE, true, std::move(InReceive));
}

FProcessOutput::~FProcessOutput() = default;

void FProcessOutput::WriteOriginal(std::string_view InText, bool bInError) noexcept
{
	(bInError ? Impl->Error : Impl->Out).Write(InText);
}

void FProcessOutput::Stop()
{
	Impl->Out.Stop();
	Impl->Error.Stop();
	if (!Impl->Out.Failure.empty() || !Impl->Error.Failure.empty())
	{
		throw std::runtime_error("Output capture failed: " + Impl->Out.Failure + Impl->Error.Failure);
	}
}

bool FProcessOutput::IsRedirected()
{
	for (DWORD Kind : {STD_OUTPUT_HANDLE, STD_ERROR_HANDLE})
	{
		const auto Handle = GetStdHandle(Kind);
		if (Valid(Handle) && (GetFileType(Handle) == FILE_TYPE_PIPE || GetFileType(Handle) == FILE_TYPE_DISK))
		{
			return true;
		}
	}
	return false;
}

void FProcessOutput::ReportFailure(std::string_view InMessage, bool bInInteractive)
{
	if (bInInteractive)
	{
		const int Length =
		    MultiByteToWideChar(CP_UTF8, 0, InMessage.data(), static_cast<int>(InMessage.size()), nullptr, 0);
		std::wstring Message(Length, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, InMessage.data(), static_cast<int>(InMessage.size()), Message.data(), Length);
		MessageBoxW(nullptr, Message.c_str(), L"Hyperion Editor", MB_OK | MB_ICONERROR);
	}
}
} // namespace Hyperion
