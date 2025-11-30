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
	enum class EPkgsType { PKGS_ENABLED, PKGS_DISABLED, PKGS_SYSTEM, PKGS_THRDPARTY, PKGS_UNINSTALLED };
	enum class EOperType { OPER_ENABLE, OPER_DISABLE, OPER_UNINSTALL, OPER_RESTORE };
	ADB() = default;
	~ADB();
	[[nodiscard]] auto GetPackagesList(std::wstring_view wsvDevice, EPkgsType eType) -> std::vector<std::wstring>;
	[[nodiscard]] auto GetDevices() -> std::vector<ADBDEVICE>;
	bool KillServer();
	void PkgOperate(std::wstring_view wsvDevice, std::wstring_view wsvPkgName, EOperType eOper);
	bool StartServer();
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
			| std::views::transform([](auto&& s) { return s | std::views::drop(8); })
			| std::ranges::to<std::vector<std::wstring>>();
	}
	std::erase_if(vecPkgs, [](const std::wstring& wstr) { return wstr.empty(); });

	return vecPkgs;
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
	default:
		break;
	}

	Execute(std::vformat(wsvFmt, std::make_wformat_args(wsvDevice, wsvPkgName)));
}

bool ADB::StartServer() {
	if (!Execute(L"adb start-server")) {
		return false;
	}

	::Sleep(200); //Delay for ADB server to start properly.
	return true;
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