module;
#include <SDKDDKVer.h>
#include <afxdialogex.h>
#include "resource.h"
export module CAPMDlgLog;

import std;
import APMUtility;

export class CAPMDlgLog : public CDialogEx {
private:
	void DoDataExchange(CDataExchange* pDX)override;
	BOOL OnInitDialog()override;
	BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)override;
	void OnOK()override;
	DECLARE_MESSAGE_MAP()
private:
	CEdit m_editLog;
};

BEGIN_MESSAGE_MAP(CAPMDlgLog, CDialogEx)
END_MESSAGE_MAP()

void CAPMDlgLog::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_APMLOG_EDIT_LOG, m_editLog);
}

BOOL CAPMDlgLog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	GetDynamicLayout()->SetMinSize({ });
	m_editLog.SetLimitText(0xFFFFUL);

	const auto hIcon = AfxGetApp()->LoadIconW(IDR_MAINFRAME);
	SetIcon(hIcon, TRUE);
	SetIcon(hIcon, FALSE);

	return TRUE;
}

BOOL CAPMDlgLog::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	const auto pHdr = reinterpret_cast<NMHDR*>(lParam);

	std::wstring wstrLog;
	switch (pHdr->code) {
	case WM_NOTIFYCODE_LOG_ADB:
	{
		const auto pAdbCmd = reinterpret_cast<ADBCMD*>(wParam);
		wstrLog = std::format(
			L"command: {}\r\n"
			L"{}"
			L"{:*<105}\r\n", pAdbCmd->wstrCMD, pAdbCmd->wstrAnswer, L"");
	}
	break;
	case WM_NOTIFYCODE_LOG_ERROR:
		wstrLog = std::format(
			L"{}\r\n"
			L"{:*<105}\r\n", reinterpret_cast<wchar_t*>(wParam), L"");
		break;
	default:
		break;
	}

	if (!wstrLog.empty()) {
		const auto iEnd = m_editLog.GetWindowTextLengthW();
		m_editLog.SetSel(iEnd, iEnd);
		m_editLog.ReplaceSel(wstrLog.data());
	}

	return CDialogEx::OnNotify(wParam, lParam, pResult);
}

void CAPMDlgLog::OnOK() { }