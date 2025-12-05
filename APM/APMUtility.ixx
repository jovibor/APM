module;
#include <Windows.h>
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

	struct ADBCMD {
		std::wstring wstrCMD;    //Command for ADB.
		std::wstring wstrAnswer; //ADB answer.
	};
}