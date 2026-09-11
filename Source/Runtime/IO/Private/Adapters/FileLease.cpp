#include "Hyperion/IO/IOService.h"
#include <windows.h>

namespace Hyperion
{
namespace
{
class FLocalWriteLease final : public IFileWriteLease
{
public:
	explicit FLocalWriteLease(HANDLE InHandle) : Handle(InHandle)
	{
	}

	~FLocalWriteLease() override
	{
		CloseHandle(Handle);
	}

private:
	HANDLE Handle;
};
} // namespace

std::shared_ptr<IFileWriteLease> FLocalFileSystem::AcquireWriteLease(const std::filesystem::path& InPath)
{
	auto Path = InPath;
	Path += ".publish-lock";
	if (!Path.parent_path().empty())
	{
		std::filesystem::create_directories(Path.parent_path());
	}
	const auto Handle = CreateFileW(Path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
	                                FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		throw std::runtime_error("Cannot acquire exclusive asset publication lease: " + InPath.generic_string());
	}
	try
	{
		return std::make_shared<FLocalWriteLease>(Handle);
	}
	catch (...)
	{
		CloseHandle(Handle);
		throw;
	}
}
} // namespace Hyperion
