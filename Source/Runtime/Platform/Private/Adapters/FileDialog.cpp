#include "Hyperion/Platform/FileDialog.h"
#include <shobjidl.h>
#include <stdexcept>
#include <windows.h>
#include <wrl/client.h>

namespace Hyperion
{
namespace
{
void CheckDialog(HRESULT InResult)
{
	if (FAILED(InResult))
	{
		throw std::runtime_error("Native folder dialog failed: " +
		                         std::to_string(static_cast<unsigned long>(InResult)));
	}
}

struct FDialogApartment
{
	HRESULT Result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	~FDialogApartment()
	{
		if (SUCCEEDED(Result))
		{
			CoUninitialize();
		}
	}
};

std::wstring DialogText(const std::string& InText)
{
	const int Size =
	    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, InText.data(), static_cast<int>(InText.size()), nullptr, 0);
	if (!Size)
	{
		throw std::invalid_argument("Invalid UTF-8 file dialog filter");
	}
	std::wstring Result(Size, L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, InText.data(), static_cast<int>(InText.size()), Result.data(),
	                    Size);
	return Result;
}
} // namespace

std::optional<std::filesystem::path> SelectFolder(FNativeSurface InOwner, const std::filesystem::path& InInitial)
{
	FDialogApartment Apartment;
	CheckDialog(Apartment.Result);
	Microsoft::WRL::ComPtr<IFileOpenDialog> Dialog;
	CheckDialog(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Dialog)));
	FILEOPENDIALOGOPTIONS Options{};
	CheckDialog(Dialog->GetOptions(&Options));
	CheckDialog(
	    Dialog->SetOptions(Options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR));
	CheckDialog(Dialog->SetTitle(L"Open asset root"));
	if (!InInitial.empty())
	{
		Microsoft::WRL::ComPtr<IShellItem> Initial;
		if (SUCCEEDED(SHCreateItemFromParsingName(InInitial.c_str(), nullptr, IID_PPV_ARGS(&Initial))))
		{
			CheckDialog(Dialog->SetFolder(Initial.Get()));
		}
	}
	const auto Result = Dialog->Show(static_cast<HWND>(InOwner.Handle));
	if (Result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
	{
		return {};
	}
	CheckDialog(Result);
	Microsoft::WRL::ComPtr<IShellItem> Item;
	CheckDialog(Dialog->GetResult(&Item));
	PWSTR Name{};
	CheckDialog(Item->GetDisplayName(SIGDN_FILESYSPATH, &Name));
	const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> Owner(Name, &CoTaskMemFree);
	return std::filesystem::path(Name);
}

std::optional<std::filesystem::path> SelectFile(FNativeSurface InOwner, const std::filesystem::path& InInitial,
                                                std::span<const FFileDialogFilter> InFilters)
{
	FDialogApartment Apartment;
	CheckDialog(Apartment.Result);
	Microsoft::WRL::ComPtr<IFileOpenDialog> Dialog;
	CheckDialog(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Dialog)));
	FILEOPENDIALOGOPTIONS Options{};
	CheckDialog(Dialog->GetOptions(&Options));
	CheckDialog(
	    Dialog->SetOptions(Options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR));
	CheckDialog(Dialog->SetTitle(L"Import Asset"));
	std::vector<std::pair<std::wstring, std::wstring>> Strings;
	for (const auto& Filter : InFilters)
	{
		Strings.emplace_back(DialogText(Filter.Name), DialogText(Filter.Pattern));
	}
	std::vector<COMDLG_FILTERSPEC> Filters;
	for (const auto& [Name, Pattern] : Strings)
	{
		Filters.push_back({Name.c_str(), Pattern.c_str()});
	}
	if (!Filters.empty())
	{
		CheckDialog(Dialog->SetFileTypes(static_cast<UINT>(Filters.size()), Filters.data()));
	}
	if (!InInitial.empty())
	{
		const auto Directory = std::filesystem::is_directory(InInitial) ? InInitial : InInitial.parent_path();
		if (std::filesystem::is_regular_file(InInitial))
		{
			CheckDialog(Dialog->SetFileName(InInitial.filename().c_str()));
		}
		Microsoft::WRL::ComPtr<IShellItem> Initial;
		if (SUCCEEDED(SHCreateItemFromParsingName(Directory.c_str(), nullptr, IID_PPV_ARGS(&Initial))))
		{
			CheckDialog(Dialog->SetFolder(Initial.Get()));
		}
	}
	const auto Status = Dialog->Show(static_cast<HWND>(InOwner.Handle));
	if (Status == HRESULT_FROM_WIN32(ERROR_CANCELLED))
	{
		return {};
	}
	CheckDialog(Status);
	Microsoft::WRL::ComPtr<IShellItem> Item;
	CheckDialog(Dialog->GetResult(&Item));
	PWSTR Name{};
	CheckDialog(Item->GetDisplayName(SIGDN_FILESYSPATH, &Name));
	const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> Owner(Name, &CoTaskMemFree);
	return std::filesystem::path(Name);
}
} // namespace Hyperion
