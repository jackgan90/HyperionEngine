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
} // namespace Hyperion
