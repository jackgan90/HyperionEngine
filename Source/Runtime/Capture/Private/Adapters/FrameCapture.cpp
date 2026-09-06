#include "Hyperion/Capture/FrameCapture.h"
#include "Hyperion/Core/Core.h"
#include <Windows.h>
#include <fstream>
#include <mutex>
#include <renderdoc_app.h>
#include <stdexcept>
#include <vector>

namespace Hyperion
{
namespace
{
std::filesystem::path FromUtf8(std::string_view InText)
{
	return std::filesystem::path(std::u8string(InText.begin(), InText.end()));
}

std::string Utf8(const std::filesystem::path& InPath)
{
	const auto Text = InPath.u8string();
	return {reinterpret_cast<const char*>(Text.data()), Text.size()};
}

std::filesystem::path DefaultLibrary()
{
	std::vector<wchar_t> Directory(32768);
	const DWORD Length =
	    GetEnvironmentVariableW(L"ProgramFiles", Directory.data(), static_cast<DWORD>(Directory.size()));
	if (!Length || Length >= Directory.size())
	{
		return L"C:\\Program Files\\RenderDoc\\renderdoc.dll";
	}
	return std::filesystem::path(Directory.data()) / L"RenderDoc/renderdoc.dll";
}

bool ValidCaptureFile(const std::filesystem::path& InPath)
{
	std::error_code Error;
	return !InPath.empty() && std::filesystem::is_regular_file(InPath, Error) &&
	       std::filesystem::file_size(InPath, Error) > 0 && !Error;
}
} // namespace

struct FFrameCapture::FImpl
{
	FFrameCaptureSettings Settings;
	mutable std::mutex Mutex;
	FFrameCaptureStatus Status;
	RENDERDOC_API_1_6_0* Api = nullptr;
	FNativeSurface Surface;
	bool bOwnsCapture = false;
	std::uint32_t CaptureCountBefore = 0;
	std::uint64_t RequestIndex = 0;
	std::string Session;
	std::filesystem::path CurrentTemplate;

	void Fail(const std::string& InMessage)
	{
		Status.State = EFrameCaptureState::Failed;
		Status.Message = InMessage;
		if (bOwnsCapture)
		{
			Status.Message += "; capture is still active, further captures disabled until restart";
		}
		Log(ELogLevel::Warning, "RenderDoc: " + Status.Message);
	}

	void DiscardOwned()
	{
		if (bOwnsCapture && Api && Api->IsFrameCapturing())
		{
			const bool bDiscarded = Api->DiscardFrameCapture(nullptr, Surface.Handle) != 0;
			if (!bDiscarded && Api->IsFrameCapturing())
			{
				// Retain ownership so Cancel/Shutdown can retry; never treat our failed cleanup as external.
				Status.bAvailable = false;
				return;
			}
		}
		bOwnsCapture = false;
	}
};

FFrameCapture::FFrameCapture(FFrameCaptureSettings InSettings) : Impl(std::make_unique<FImpl>())
{
	Impl->Settings = std::move(InSettings);
}

FFrameCapture::~FFrameCapture()
{
	Shutdown();
}

void FFrameCapture::Initialize()
{
	auto& P = *Impl;
	std::lock_guard Lock(P.Mutex);
	if (P.Status.bAvailable || P.bOwnsCapture)
	{
		return;
	}
	try
	{
		HMODULE Module = GetModuleHandleW(L"renderdoc.dll");
		if (!Module)
		{
			const auto Library =
			    P.Settings.LibraryPath.empty() ? DefaultLibrary() : std::filesystem::absolute(P.Settings.LibraryPath);
			Module = LoadLibraryExW(Library.c_str(), nullptr,
			                        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
			if (!Module)
			{
				const DWORD Error = GetLastError();
				throw std::runtime_error("Cannot load " + Utf8(Library) + " (Windows error " + std::to_string(Error) +
				                         ")");
			}
		}
		// No FreeLibrary/RemoveHooks: wrapped graphics objects can survive plugin Stop.
		const auto GetApi = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(Module, "RENDERDOC_GetAPI"));
		if (!GetApi || !GetApi(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&P.Api)) || !P.Api)
		{
			throw std::runtime_error("Runtime does not provide RenderDoc application API 1.6.0");
		}
		if (P.Settings.OutputDirectory.empty())
		{
			P.Settings.OutputDirectory = "out/captures/renderdoc";
		}
		P.Settings.OutputDirectory = std::filesystem::absolute(P.Settings.OutputDirectory).lexically_normal();
		if (P.Settings.Prefix.empty() || P.Settings.Prefix.find_first_of("/\\:*?\"<>|") != std::string::npos)
		{
			throw std::runtime_error("Capture prefix must be a nonempty filename component");
		}
		P.Session =
		    P.Settings.Prefix + "-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(ClockNanoseconds());
		int Major = 0;
		int Minor = 0;
		int Patch = 0;
		P.Api->GetAPIVersion(&Major, &Minor, &Patch);
		P.Status.State = EFrameCaptureState::Ready;
		P.Status.bAvailable = true;
		P.Status.Message =
		    "Ready (API " + std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch) + ")";
		Log(ELogLevel::Info, "RenderDoc: " + P.Status.Message);
	}
	catch (const std::exception& Error)
	{
		P.Api = nullptr;
		P.Status.bAvailable = false;
		P.Status.State = EFrameCaptureState::Unavailable;
		P.Status.Message = Error.what();
		Log(ELogLevel::Warning, "RenderDoc unavailable: " + P.Status.Message);
	}
}

void FFrameCapture::Shutdown() noexcept
{
	try
	{
		auto& P = *Impl;
		std::lock_guard Lock(P.Mutex);
		P.DiscardOwned();
		P.Status.bAvailable = false;
		if (P.bOwnsCapture)
		{
			P.Fail("RenderDoc could not stop the owned capture");
			return;
		}
		P.Status.State = EFrameCaptureState::Disabled;
		P.Status.Message = "RenderDoc stopped; restart the application to remove capture hooks";
	}
	catch (...)
	{
	}
}

FFrameCaptureStatus FFrameCapture::Status() const
{
	std::lock_guard Lock(Impl->Mutex);
	return Impl->Status;
}

bool FFrameCapture::RequestCapture()
{
	auto& P = *Impl;
	std::lock_guard Lock(P.Mutex);
	if (!P.Status.bAvailable || P.Status.State == EFrameCaptureState::Pending || P.bOwnsCapture)
	{
		return false;
	}
	if (P.Api->IsFrameCapturing())
	{
		P.Fail("Another RenderDoc capture is already active");
		return false;
	}
	P.Status.State = EFrameCaptureState::Pending;
	P.Status.Message = "Waiting for a drawable frame";
	P.Status.ReplayMessage.clear();
	return true;
}

bool FFrameCapture::BeginFrame(FNativeSurface InSurface)
{
	auto& P = *Impl;
	std::lock_guard Lock(P.Mutex);
	if (P.Status.State != EFrameCaptureState::Pending)
	{
		return false;
	}
	try
	{
		if (P.Api->IsFrameCapturing())
		{
			throw std::runtime_error("Another RenderDoc capture is already active");
		}
		std::filesystem::create_directories(P.Settings.OutputDirectory);
		P.CurrentTemplate = P.Settings.OutputDirectory / FromUtf8(P.Session + "-" + std::to_string(++P.RequestIndex));
		// Detect write failures before starting. The unique probe is ours, never a capture.
		auto Probe = P.CurrentTemplate;
		Probe += ".tmp";
		{
			std::ofstream Stream(Probe, std::ios::binary);
			Stream.put('H');
			Stream.close();
			if (!Stream)
			{
				std::error_code Ignored;
				std::filesystem::remove(Probe, Ignored);
				throw std::runtime_error("Capture output is not writable: " + Utf8(P.Settings.OutputDirectory));
			}
		}
		std::filesystem::remove(Probe);
		P.Api->SetCaptureFilePathTemplate(Utf8(P.CurrentTemplate).c_str());
		P.CaptureCountBefore = P.Api->GetNumCaptures();
		P.Surface = InSurface;
		P.Api->StartFrameCapture(nullptr, InSurface.Handle);
		if (!P.Api->IsFrameCapturing())
		{
			throw std::runtime_error("No hooked graphics target; enable RenderDoc before device creation and restart");
		}
		P.bOwnsCapture = true;
		P.Status.State = EFrameCaptureState::Capturing;
		P.Status.Message = "Capturing frame";
		return true;
	}
	catch (const std::exception& Error)
	{
		P.DiscardOwned();
		P.Fail(Error.what());
		return false;
	}
}

bool FFrameCapture::EndFrame()
{
	auto& P = *Impl;
	std::lock_guard Lock(P.Mutex);
	if (!P.bOwnsCapture)
	{
		return false;
	}
	try
	{
		const bool bSuccess = P.Api->EndFrameCapture(nullptr, P.Surface.Handle) != 0;
		P.bOwnsCapture = !bSuccess && P.Api->IsFrameCapturing();
		if (!bSuccess)
		{
			throw std::runtime_error("RenderDoc failed to save the frame");
		}
		const std::uint32_t Count = P.Api->GetNumCaptures();
		for (std::uint32_t Index = P.CaptureCountBefore; Index < Count; ++Index)
		{
			std::uint32_t Length = 0;
			if (!P.Api->GetCapture(Index, nullptr, &Length, nullptr) || !Length)
			{
				continue;
			}
			std::vector<char> Filename(Length + 1, 0);
			if (!P.Api->GetCapture(Index, Filename.data(), &Length, nullptr))
			{
				continue;
			}
			const auto Path = std::filesystem::absolute(FromUtf8(Filename.data())).lexically_normal();
			if (Path.parent_path() != P.CurrentTemplate.parent_path() || Path.extension() != ".rdc" ||
			    !Utf8(Path.filename()).starts_with(Utf8(P.CurrentTemplate.filename()) + "_") || !ValidCaptureFile(Path))
			{
				continue;
			}
			P.Status.LastCapture = Path;
			++P.Status.CompletedCaptures;
			P.Status.State = EFrameCaptureState::Succeeded;
			P.Status.Message = "Capture saved";
			Log(ELogLevel::Info, "RenderDoc capture saved: " + Utf8(Path));
			return true;
		}
		throw std::runtime_error("RenderDoc returned no new nonempty capture file for this request");
	}
	catch (const std::exception& Error)
	{
		P.DiscardOwned();
		P.Fail(Error.what());
		return false;
	}
}

void FFrameCapture::Cancel() noexcept
{
	try
	{
		auto& P = *Impl;
		std::lock_guard Lock(P.Mutex);
		if (P.bOwnsCapture || P.Status.State == EFrameCaptureState::Pending)
		{
			P.DiscardOwned();
			P.Fail(P.bOwnsCapture         ? "RenderDoc could not cancel the owned capture"
			       : !P.Status.bAvailable ? "Capture cancelled; restart the application to resume capturing"
			                              : "Capture cancelled before the frame completed");
		}
	}
	catch (...)
	{
	}
}

bool FFrameCapture::OpenLastCapture()
{
	auto& P = *Impl;
	std::lock_guard Lock(P.Mutex);
	P.Status.ReplayProcessId = 0;
	try
	{
		if (!P.Status.bAvailable || !ValidCaptureFile(P.Status.LastCapture))
		{
			throw std::runtime_error("No saved capture is available to open");
		}
		const std::string CommandLine = "\"" + Utf8(P.Status.LastCapture) + "\"";
		P.Status.ReplayProcessId = P.Api->LaunchReplayUI(0, CommandLine.c_str());
		if (!P.Status.ReplayProcessId)
		{
			throw std::runtime_error("Could not launch the RenderDoc UI associated with the loaded runtime");
		}
		P.Status.ReplayMessage = "Opened capture in RenderDoc";
		Log(ELogLevel::Info, "RenderDoc replay launched: " + std::to_string(P.Status.ReplayProcessId) + "; " +
		                         Utf8(P.Status.LastCapture));
		return true;
	}
	catch (const std::exception& Error)
	{
		P.Status.ReplayMessage = Error.what();
		Log(ELogLevel::Warning, "RenderDoc replay: " + P.Status.ReplayMessage);
		return false;
	}
}
} // namespace Hyperion
