module;
#include <Windows.h>
export module ADB;

import std;

using std::operator""sv;

[[nodiscard]] auto StrToWstr(std::string_view sv, UINT uCodePage = CP_UTF8) -> std::wstring
{
	const auto iSize = ::MultiByteToWideChar(uCodePage, 0, sv.data(), static_cast<int>(sv.size()), nullptr, 0);
	std::wstring wstr;
	wstr.resize_and_overwrite(iSize, [=](wchar_t* pwbuff, std::size_t sizeBuff) {
		::MultiByteToWideChar(uCodePage, 0, sv.data(), static_cast<int>(sv.size()), pwbuff, static_cast<int>(sizeBuff));
		return iSize;
	});

	return wstr;
}

export class ADB {
public:
	struct ADBDEVICE {
		std::wstring wstrName;
		std::wstring wstrModel;
	};
	ADB() = default;
	~ADB();
	[[nodiscard]] auto GetAllInstalledPackages(std::wstring_view wsvDevice) -> std::vector<std::wstring>;
	[[nodiscard]] auto GetDevices() -> std::vector<ADBDEVICE>;
	bool KillServer();
	bool StartServer();
	void UninstallPkg(std::wstring_view wsvDevice, std::wstring_view wsvPkgName);
private:
	bool Execute(std::wstring_view wsvCMD);
	void CloseHandles();
	[[nodiscard]] auto ReadFromPipe() -> std::wstring;
private:
	HANDLE m_hADBSTDOutRO { };
	HANDLE m_hADBSTDOutWR { };
};

ADB::~ADB() {
	CloseHandles();
}

auto ADB::GetAllInstalledPackages(std::wstring_view wsvDevice)->std::vector<std::wstring>
{
	Execute(std::format(L"adb -s {} shell pm list packages", wsvDevice).data());
	auto vecPKGS = ReadFromPipe() | std::views::split(L"\r\n"sv)
		| std::views::transform([](auto&& s) { return s | std::views::drop(8); })
		| std::ranges::to<std::vector<std::wstring>>();
	std::erase_if(vecPKGS, [](const std::wstring& wstr) {return wstr.empty(); });

	return vecPKGS;
}

auto ADB::GetDevices()->std::vector<ADBDEVICE>
{
	if (!StartServer()) {
		::MessageBoxW(0, L"\"adb start-server\" failed.", 0, MB_ICONERROR);
		return { };
	}

	if (!Execute(L"adb devices")) {
		::MessageBoxW(0, L"\"adb devices\" failed.", 0, MB_ICONERROR);
		return { };
	}

	//Drop first text line: "List of devices attached\r\n"
	auto vecNames = ReadFromPipe() | std::views::drop(26) | std::views::split(L"\tdevice\r\n"sv)
		| std::ranges::to<std::vector<std::wstring>>();

	//Device name is printed as 8 symbols, so erase any "CRLF" or other garbage.
	std::erase_if(vecNames, [](const std::wstring& wstr) { return wstr.size() < 8; });

	std::vector<ADBDEVICE> vecRet;
	for (const auto& wstrName : vecNames) {
		Execute(std::format(L"adb -s {} shell getprop ro.product.model", wstrName).data());
		vecRet.emplace_back(ADBDEVICE { .wstrName { std::move(wstrName) }, .wstrModel { ReadFromPipe() } });
	}

	return vecRet;
}

bool ADB::KillServer() {
	return Execute(L"adb kill-server");
}

bool ADB::StartServer() {
	if (!Execute(L"adb start-server")) {
		return false;
	}

	::Sleep(200); //Delay for ADB server to start properly.
	return true;
}

void ADB::UninstallPkg(std::wstring_view wsvDevice, std::wstring_view wsvPkgName) {
	Execute(std::format(L"adb -s {} shell pm uninstall --user 0 {}", wsvDevice, wsvPkgName));
}


//Private methods.

bool ADB::Execute(std::wstring_view wsvCMD)
{
	CloseHandles();

	SECURITY_ATTRIBUTES sa { .nLength { sizeof(SECURITY_ATTRIBUTES) }, .bInheritHandle { TRUE } };
	::CreatePipe(&m_hADBSTDOutRO, &m_hADBSTDOutWR, &sa, 0);
	::SetHandleInformation(m_hADBSTDOutRO, HANDLE_FLAG_INHERIT, 0);

	PROCESS_INFORMATION pi { };
	STARTUPINFOW si { .cb { sizeof(STARTUPINFOW) }, .dwFlags { STARTF_USESTDHANDLES }, .hStdOutput { m_hADBSTDOutWR },
		.hStdError { m_hADBSTDOutWR } };

	std::wstring wstrProc = L"ADB\\" + std::wstring { wsvCMD };
	if (::CreateProcessW(nullptr, wstrProc.data(), nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi) == FALSE) {
		CloseHandles();
		return false;
	}

	::MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, INFINITE, QS_ALLINPUT);
	::CloseHandle(pi.hProcess);
	::CloseHandle(pi.hThread);
	::CloseHandle(m_hADBSTDOutWR);
	m_hADBSTDOutWR = nullptr;

	return true;
}

void ADB::CloseHandles()
{
	if (m_hADBSTDOutWR != nullptr) {
		::CloseHandle(m_hADBSTDOutWR);
	}

	if (m_hADBSTDOutRO != nullptr) {
		::CloseHandle(m_hADBSTDOutRO);
	}

	m_hADBSTDOutWR = nullptr;
	m_hADBSTDOutRO = nullptr;
}

auto ADB::ReadFromPipe()->std::wstring
{
	constexpr auto sizeBuff { 256 };
	char chBuf[sizeBuff];
	std::string str;

	for (;;) {
		DWORD dwRead;
		const auto bSuccess = ::ReadFile(m_hADBSTDOutRO, chBuf, sizeBuff, &dwRead, nullptr);
		if (!bSuccess || dwRead == 0) {
			break;
		}

		str += std::string(chBuf, dwRead);
	}

	::CloseHandle(m_hADBSTDOutRO);
	m_hADBSTDOutRO = nullptr;

	return StrToWstr(str);
}