module;
#include <Windows.h>
#include <ShObjIdl_core.h>
export module APMUtility;

import std;

export {
	constexpr auto WM_NOTIFYCODE_LOG_ADB = WM_USER + 1;
	constexpr auto WM_NOTIFYCODE_LOG_ERROR = WM_NOTIFYCODE_LOG_ADB + 1;

	[[nodiscard]] auto StrToWstr(std::string_view sv, UINT uCodePage = CP_UTF8) -> std::wstring {
		const auto iSize = ::MultiByteToWideChar(uCodePage, 0, sv.data(), static_cast<int>(sv.size()), nullptr, 0);
		std::wstring wstr;
		wstr.resize_and_overwrite(iSize, [=](wchar_t* pwbuff, std::size_t sizeBuff) {
			::MultiByteToWideChar(uCodePage, 0, sv.data(), static_cast<int>(sv.size()), pwbuff, static_cast<int>(sizeBuff));
			return iSize;
		});

		return wstr;
	}

	[[nodiscard]] auto DlgGetFolderPath(bool fQuote = false) -> std::optional<std::wstring> {
		IFileOpenDialog *pIFOD { };
		if (::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIFOD)) != S_OK) {
			return std::nullopt;
		}

		DWORD dwFlags;
		pIFOD->GetOptions(&dwFlags);
		pIFOD->SetOptions(dwFlags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_DONTADDTORECENT | FOS_PATHMUSTEXIST);

		if (pIFOD->Show(nullptr) != S_OK) { //Cancel was pressed.
			return std::nullopt;
		}

		IShellItem* pShellItem { };
		if (pIFOD->GetResult(&pShellItem); pShellItem == nullptr) {
			return std::nullopt;
		}

		std::optional<std::wstring> optRet;
		wchar_t* pwszPath { };
		if (pShellItem->GetDisplayName(SIGDN_FILESYSPATH, &pwszPath) != S_OK) {
			return std::nullopt;
		}

		//Put path in quotes or not?
		optRet = fQuote ? L'\"' + std::wstring { pwszPath } + L'\"' : pwszPath;
		::CoTaskMemFree(pwszPath);
		pShellItem->Release();
		pIFOD->Release();

		return optRet;
	}

	struct ADBCMD {
		std::wstring wstrCMD;    //Command for ADB.
		std::wstring wstrAnswer; //ADB answer.
	};
}