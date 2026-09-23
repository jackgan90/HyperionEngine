#include "../StdioReader.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <stdexcept>

namespace Hyperion
{
struct FStdioReader::FImpl
{
	HANDLE Input = GetStdHandle(STD_INPUT_HANDLE);
	DWORD Kind = GetFileType(Input);
	bool bEof{};
};

FStdioReader::FStdioReader() : Impl(std::make_unique<FImpl>())
{
	if (Impl->Kind != FILE_TYPE_PIPE && Impl->Kind != FILE_TYPE_DISK)
	{
		throw std::invalid_argument(
		    "Session modes require redirected stdin (pipe or file); use --help for one-shot commands");
	}
}

FStdioReader::~FStdioReader() = default;

bool FStdioReader::IsEof() const
{
	return Impl->bEof;
}

std::string FStdioReader::Read()
{
	if (Impl->bEof)
	{
		return {};
	}
	std::array<char, 16384> Buffer;
	DWORD Available = static_cast<DWORD>(Buffer.size());
	if (Impl->Kind == FILE_TYPE_PIPE && !PeekNamedPipe(Impl->Input, nullptr, 0, nullptr, &Available, nullptr))
	{
		if (GetLastError() != ERROR_BROKEN_PIPE)
		{
			throw std::runtime_error("Could not inspect stdin pipe");
		}
		Impl->bEof = true;
		return {};
	}
	if (!Available)
	{
		return {};
	}
	DWORD Read{};
	if (!ReadFile(Impl->Input, Buffer.data(), std::min(Available, static_cast<DWORD>(Buffer.size())), &Read, nullptr))
	{
		if (GetLastError() != ERROR_BROKEN_PIPE)
		{
			throw std::runtime_error("Could not read stdin");
		}
		Impl->bEof = true;
		return {};
	}
	Impl->bEof = Read == 0;
	return {Buffer.data(), Read};
}
} // namespace Hyperion
