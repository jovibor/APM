module;
#include <Windows.h>
export module ADB;

import APMUtility;
import std;

using std::operator""sv;

export class ADB {
public:
	struct ADBDEVICE {
		std::wstring wstrSerialNumber;
		std::wstring wstrModelName;
		std::wstring wstrAndroidVer;
	};
	enum class EPkgsType { PKGS_ENABLED, PKGS_DISABLED, PKGS_SYSTEM, PKGS_THRDPARTY, PKGS_UNINSTALLED };
	enum class EOperType { OPER_ENABLE, OPER_DISABLE, OPER_UNINSTALL, OPER_RESTORE, OPER_INSTALL };
	ADB() = default;
	~ADB();
	void AddLoggerHWND(HWND hWndLog);
	[[nodiscard]] auto GetPackagesList(std::wstring_view wsvDevice, EPkgsType eType) -> std::vector<std::wstring>;
	[[nodiscard]] auto GetDevices() -> std::vector<ADBDEVICE>;
	bool KillServer();
	void PkgOperate(std::wstring_view wsvDevice, std::wstring_view wsvPkgName, EOperType eOper);
	bool StartServer();
private:
	void AddLogEntry(const ADBCMD& adbCmd);
	void AddLogEntry(const wchar_t* pwsz);
	void CloseHandles();
	bool Execute(std::wstring_view wsvCMD);
	[[nodiscard]] auto ReadFromPipe() -> std::wstring;
private:
	HANDLE m_hADBSTDOutRO { };
	HANDLE m_hADBSTDOutWR { };
	HWND m_hWndLog { };
};

ADB::~ADB() {
	CloseHandles();
}

void ADB::AddLoggerHWND(HWND hWndLog) {
	m_hWndLog = hWndLog;
}

auto ADB::GetPackagesList(std::wstring_view wsvDevice, EPkgsType eType)->std::vector<std::wstring>
{
	using enum EPkgsType;
	std::vector<std::wstring> vecPkgs;
	if (eType == PKGS_UNINSTALLED) {
		Execute(std::format(L"adb -s {} shell pm list packages -e", wsvDevice).data());
		const auto vecEnabled = ReadFromPipe() | std::views::split(L"\r\n"sv)
			| std::ranges::to<std::vector<std::wstring>>();

		Execute(std::format(L"adb -s {} shell pm list packages -u", wsvDevice).data());
		auto vecEnabledWithUninst = ReadFromPipe() | std::views::split(L"\r\n"sv) //Deliberately non const.
			| std::ranges::to<std::vector<std::wstring>>();

		vecPkgs = vecEnabledWithUninst | std::views::filter([&](const std::wstring& wstr) {
			return std::ranges::find(vecEnabled, wstr) == vecEnabled.end();	})
			| std::views::as_rvalue
			| std::views::transform([](auto&& s) { return s | std::views::drop(8); })
			| std::ranges::to<std::vector<std::wstring>>();
	}
	else {
		std::wstring_view wsvCmdPkgType;
		switch (eType) {
		case PKGS_ENABLED:
			wsvCmdPkgType = L"-e";
			break;
		case PKGS_DISABLED:
			wsvCmdPkgType = L"-d";
			break;
		case PKGS_SYSTEM:
			wsvCmdPkgType = L"-s";
			break;
		case PKGS_THRDPARTY:
			wsvCmdPkgType = L"-3";
			break;
		default:
			break;
		}

		Execute(std::format(L"adb -s {} shell pm list packages {}", wsvDevice, wsvCmdPkgType).data());
		vecPkgs = ReadFromPipe() | std::views::split(L"\r\n"sv)
			| std::views::transform([](auto&& s) { return s | std::views::drop(8); }) //drop "package:".
			| std::ranges::to<std::vector<std::wstring>>();
	}

	std::erase_if(vecPkgs, [](const std::wstring& wstr) { return wstr.empty(); });
	for (auto& wstr : vecPkgs) {
		if (const auto it = std::ranges::find_if(wstr, [](const wchar_t wch) {
			return wch < 0x20 || wch > 0x7E; }); it != wstr.end()) { //Find any non printable wchars.
			wstr.resize(it - wstr.begin());
		}
	}

	return vecPkgs;
}

auto ADB::GetDevices()->std::vector<ADBDEVICE>
{
	if (!StartServer()) {
		return { };
	}

	constexpr auto pwszCmdDev = L"adb devices";
	if (!Execute(pwszCmdDev)) {
		return { };
	}

	ADBCMD logDev { .wstrCMD { pwszCmdDev }, .wstrAnswer { ReadFromPipe() } };
	AddLogEntry(logDev);

	//Drop first text line: "List of devices attached\r\n", and split on CRLF.
	auto vecNamesAll = logDev.wstrAnswer | std::views::drop(26) | std::views::split(L"\r\n"sv)
		| std::ranges::to<std::vector<std::wstring>>();

	//Device name is printed as, at least, 8 symbols, so erase any empty strings and offline devices.
	std::erase_if(vecNamesAll, [](const std::wstring& wstr) { return (wstr.size() < 8) || wstr.contains(L"offline"); });

	std::vector<ADBDEVICE> vecRet;
	for (const auto& wstr : vecNamesAll) {
		if (const auto sPos = wstr.find(L"\tdevice"); sPos != std::wstring::npos) {
			const auto wsvDevName = std::wstring_view(wstr).substr(0, sPos);
			ADBDEVICE adbDevice { .wstrSerialNumber { wsvDevName } };

			logDev.wstrCMD = std::format(L"adb -s {} shell getprop ro.product.model", wsvDevName);
			if (!Execute(logDev.wstrCMD)) { break; }

			logDev.wstrAnswer = ReadFromPipe();
			AddLogEntry(logDev);
			adbDevice.wstrModelName = std::move(logDev.wstrAnswer);

			logDev.wstrCMD = std::format(L"adb -s {} shell getprop ro.build.version.release", wsvDevName);
			if (!Execute(logDev.wstrCMD)) { break; }

			logDev.wstrAnswer = ReadFromPipe();
			AddLogEntry(logDev);
			adbDevice.wstrAndroidVer = std::move(logDev.wstrAnswer);

			vecRet.emplace_back(adbDevice);
		}
	}

	return vecRet;
}

bool ADB::KillServer() {
	constexpr auto pwszCmd = L"adb kill-server";
	if (!Execute(pwszCmd)) {
		return false;
	}

	AddLogEntry({ .wstrCMD { pwszCmd }, .wstrAnswer { ReadFromPipe() } });

	return true;
}

void ADB::PkgOperate(std::wstring_view wsvDevice, std::wstring_view wsvPkgName, EOperType eOper)
{
	using enum EOperType;

	std::wstring_view wsvFmt;
	switch (eOper) {
	case OPER_ENABLE:
		wsvFmt = L"adb -s {} shell pm enable --user 0 {}";
		break;
	case OPER_DISABLE:
		wsvFmt = L"adb -s {} shell pm disable-user --user 0 {}";
		break;
	case OPER_UNINSTALL:
		wsvFmt = L"adb -s {} shell pm uninstall --user 0 {}";
		break;
	case OPER_RESTORE:
		wsvFmt = L"adb -s {} shell cmd package install-existing {}";
		break;
	case OPER_INSTALL:
		wsvFmt = L"adb -s {} install --user 0 {}";
		break;
	default:
		return;
	}

	ADBCMD adbCmd { .wstrCMD { std::vformat(wsvFmt, std::make_wformat_args(wsvDevice, wsvPkgName)) } };
	Execute(adbCmd.wstrCMD);
	adbCmd.wstrAnswer = ReadFromPipe();
	AddLogEntry(adbCmd);
}

bool ADB::StartServer() {
	constexpr auto pwszCmd = L"adb start-server";
	if (!Execute(pwszCmd)) {
		return false;
	}

	AddLogEntry({ .wstrCMD { pwszCmd }, .wstrAnswer { ReadFromPipe() } });
	::Sleep(300); //Delay for ADB server to start properly.

	return true;
}


//Private methods.

void ADB::AddLogEntry(const ADBCMD& adbCmd)
{
	static const auto hWnd = ::GetDesktopWindow();
	const NMHDR hdr { .hwndFrom { hWnd }, .code { WM_NOTIFYCODE_LOG_ADB } };
	::SendMessageW(m_hWndLog, WM_NOTIFY, reinterpret_cast<WPARAM>(&adbCmd), reinterpret_cast<LPARAM>(&hdr));
}

void ADB::AddLogEntry(const wchar_t* pwsz)
{
	const NMHDR hdr { .hwndFrom { ::GetDesktopWindow() }, .code { WM_NOTIFYCODE_LOG_ERROR } };
	::SendMessageW(m_hWndLog, WM_NOTIFY, reinterpret_cast<WPARAM>(pwsz), reinterpret_cast<LPARAM>(&hdr));
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
		const auto wstrErr = L"CreateProcessW failed with: " + wstrProc;
		AddLogEntry(wstrErr.data());
		return false;
	}

	::MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, INFINITE, QS_ALLINPUT);
	::CloseHandle(pi.hProcess);
	::CloseHandle(pi.hThread);
	::CloseHandle(m_hADBSTDOutWR);
	m_hADBSTDOutWR = nullptr;

	return true;
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