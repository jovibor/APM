module;
#include <SDKDDKVer.h>
#include <afxwin.h>
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
export module CAPMApp;

import CAPMDlg;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

class CAPMApp : public CWinApp {
public:
	CAPMApp() = default;
private:
	BOOL InitInstance()override;
	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CAPMApp, CWinApp)
END_MESSAGE_MAP()

CAPMApp theApp;

BOOL CAPMApp::InitInstance()
{
	CWinApp::InitInstance();

	SetRegistryKey(L"Android Package Manager");

	CAPMDlg dlg;
	m_pMainWnd = &dlg;
	dlg.DoModal();

	return FALSE;
}