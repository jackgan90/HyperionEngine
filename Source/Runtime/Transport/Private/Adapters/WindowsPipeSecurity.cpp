#include "WindowsPipeInternal.h"
#include <sddl.h>

namespace Hyperion
{
namespace
{
std::vector<std::byte> ReadTokenUser(HANDLE InProcess)
{
	FPipeHandle Token;
	if (!OpenProcessToken(InProcess, TOKEN_QUERY, &Token.Value))
	{
		throw FTransportError("access_denied", "Could not read pipe peer identity");
	}
	DWORD Size{};
	GetTokenInformation(Token.Value, TokenUser, nullptr, 0, &Size);
	std::vector<std::byte> Buffer(Size);
	if (!Size || !GetTokenInformation(Token.Value, TokenUser, Buffer.data(), Size, &Size))
	{
		throw FTransportError("access_denied", "Could not read user token");
	}
	return Buffer;
}
} // namespace

std::wstring CurrentPipeUser()
{
	const auto User = ReadTokenUser(GetCurrentProcess());
	LPWSTR Text{};
	if (!ConvertSidToStringSidW(reinterpret_cast<const TOKEN_USER*>(User.data())->User.Sid, &Text))
	{
		throw FTransportError("access_denied", "Could not encode user identity");
	}
	std::wstring Result(Text);
	LocalFree(Text);
	return Result;
}

FPipeSecurity::FPipeSecurity()
{
	const auto Descriptor = L"D:P(A;OICI;GA;;;" + CurrentPipeUser() + L")";
	Attributes.nLength = sizeof(Attributes);
	if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(Descriptor.c_str(), SDDL_REVISION_1,
	                                                          &Attributes.lpSecurityDescriptor, nullptr))
	{
		throw FTransportError("access_denied", "Could not create current-user pipe access policy");
	}
}

FPipeSecurity::~FPipeSecurity()
{
	LocalFree(Attributes.lpSecurityDescriptor);
}

void VerifyPipeServerUser(HANDLE InPipe)
{
	ULONG ProcessId{};
	if (!GetNamedPipeServerProcessId(InPipe, &ProcessId))
	{
		throw FTransportError("access_denied", "Could not identify pipe server");
	}
	FPipeHandle Process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ProcessId));
	if (Process.Value == nullptr)
	{
		throw FTransportError("access_denied", "Could not verify pipe server process");
	}
	const auto Server = ReadTokenUser(Process.Value);
	const auto Client = ReadTokenUser(GetCurrentProcess());
	if (!EqualSid(reinterpret_cast<const TOKEN_USER*>(Server.data())->User.Sid,
	              reinterpret_cast<const TOKEN_USER*>(Client.data())->User.Sid))
	{
		throw FTransportError("access_denied", "Pipe server belongs to another user");
	}
}

std::wstring NativePipeName(const std::string& InAddress)
{
	if (InAddress.empty() || InAddress.size() > 160 ||
	    InAddress.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.") !=
	        std::string::npos)
	{
		throw FTransportError("invalid_address", "Expected a local Hyperion pipe name");
	}
	return L"\\\\.\\pipe\\Hyperion." + std::wstring(InAddress.begin(), InAddress.end());
}

FTransportAddress LocalTransportAddress(const std::string& InInstance)
{
	return {"npipe", InInstance};
}

std::filesystem::path LocalDiscoveryDirectory()
{
	wchar_t Buffer[32768]{};
	const auto Size = GetEnvironmentVariableW(L"LOCALAPPDATA", Buffer, 32768);
	if (!Size || Size >= 32768)
	{
		throw FTransportError("discovery_unavailable", "LOCALAPPDATA is unavailable");
	}
	const auto Directory = std::filesystem::path(Buffer) / "Hyperion" / "Automation" / CurrentPipeUser();
	std::filesystem::create_directories(Directory);
	FPipeSecurity Security;
	if (!SetFileSecurityW(Directory.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
	                      Security.Attributes.lpSecurityDescriptor))
	{
		throw FTransportError("access_denied", "Could not restrict target discovery to the current user");
	}
	return Directory;
}
} // namespace Hyperion
