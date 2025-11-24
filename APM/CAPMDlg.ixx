module;
#include <SDKDDKVer.h>
#include <afxdialogex.h>
#include "resource.h"
export module CAPMDlg;

import ListEx;
import ADB;
import std;

constexpr auto APM_VERSION_MAJOR = 1;
constexpr auto APM_VERSION_MINOR = 0;
constexpr auto APM_VERSION_PATCH = 0;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

class CAboutDlg : public CDialogEx {
public:
	CAboutDlg() : CDialogEx(IDD_ABOUT) { };
private:
	BOOL OnInitDialog()override;
	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

auto CAboutDlg::OnInitDialog()->BOOL
{
	CDialogEx::OnInitDialog();

	const auto wstrVersion = std::format(L"Android Package Manager, v{}.{}.{}",
		APM_VERSION_MAJOR, APM_VERSION_MINOR, APM_VERSION_PATCH);
	GetDlgItem(IDC_ABOUT_STAT_VERSION)->SetWindowTextW(wstrVersion.data());

	return TRUE;
}


export class CAPMDlg : public CDialogEx {
public:
	CAPMDlg(CWnd* pParent = nullptr) : CDialogEx(IDD_APM, pParent) { }
private:
	void DoDataExchange(CDataExchange* pDX)override;
	BOOL OnInitDialog()override;
	afx_msg void OnBtnDevices();
	afx_msg void OnBtnListPkgs();
	afx_msg void OnBtnUninstPkgs();
	afx_msg void OnComboDevicesSelChange();
	afx_msg void OnDestroy();
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT pDIS);
	void OnDeviceListUpdated();
	afx_msg void OnEditFilterChange();
	afx_msg void OnListColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT pMIS);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	[[nodiscard]] auto GetCurrDevice() -> const ADB::ADBDEVICE&;
	void RecalcIndexes();
	DECLARE_MESSAGE_MAP()
private:
	LISTEX::CListEx m_list;
	ADB m_adb;
	std::vector<ADB::ADBDEVICE> m_vecDevices;
	std::vector<std::wstring> m_vecPkgs;
	std::vector<std::size_t> m_vecPkgsIndexes;
	CComboBox m_cmbDevices;
};

BEGIN_MESSAGE_MAP(CAPMDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_DEVICES, &CAPMDlg::OnBtnDevices)
	ON_BN_CLICKED(IDC_BTN_PKGS_LIST, &CAPMDlg::OnBtnListPkgs)
	ON_BN_CLICKED(IDC_BTN_PKGS_UNINSTALL, &CAPMDlg::OnBtnUninstPkgs)
	ON_CBN_SELCHANGE(IDC_CMB_DEVICES, &CAPMDlg::OnComboDevicesSelChange)
	ON_EN_CHANGE(IDC_EDIT_FILTER, &CAPMDlg::OnEditFilterChange)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_LIST, &CAPMDlg::OnListColumnClick)
	ON_NOTIFY(LVN_GETDISPINFOW, IDC_LIST, &CAPMDlg::OnListGetDispInfo)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST, &CAPMDlg::OnListItemChanged)
	ON_WM_DESTROY()
	ON_WM_DRAWITEM()
	ON_WM_MEASUREITEM()
	ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

void CAPMDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CMB_DEVICES, m_cmbDevices);
}

BOOL CAPMDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_list.Create({ .hWndParent { m_hWnd }, .uID { IDC_LIST }, .fDialogCtrl { true }, .fSortable { true } });
	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	m_list.InsertColumn(0, L"Packages", 0, 700);

	const auto uCloseServer = AfxGetApp()->GetProfileIntW(L"", L"close-adb-server-on-exit", 0);
	CheckDlgButton(IDC_CHK_CLOSESRVONEXIT, uCloseServer > 0 ? BST_CHECKED : BST_UNCHECKED);

	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);
	if (const auto pSysMenu = GetSystemMenu(FALSE); pSysMenu != nullptr) {
		pSysMenu->AppendMenuW(MF_SEPARATOR);
		pSysMenu->AppendMenuW(MF_STRING, IDM_ABOUTBOX, L"About...");
	}

	const auto hIcon = AfxGetApp()->LoadIconW(IDR_MAINFRAME);
	SetIcon(hIcon, TRUE);
	SetIcon(hIcon, FALSE);

	//Scan for devices at dialog startup in separate thread, to not linger GUI appearance.
	std::thread thrdDevices(&CAPMDlg::OnBtnDevices, this);
	thrdDevices.detach();

	return TRUE;
}

void CAPMDlg::OnBtnDevices()
{
	const auto pBtnDevs = GetDlgItem(IDC_BTN_DEVICES);
	pBtnDevs->EnableWindow(FALSE);

	m_cmbDevices.ResetContent();
	m_vecDevices = m_adb.GetDevices();

	if (!m_vecDevices.empty()) {
		for (const auto& [index, dev] : std::views::enumerate(m_vecDevices)) {
			m_cmbDevices.SetItemData(m_cmbDevices.AddString(dev.wstrModel.data()), index);
		}

		m_cmbDevices.SetCurSel(0);
	}

	OnDeviceListUpdated();
	pBtnDevs->EnableWindow(TRUE);
}

void CAPMDlg::OnBtnListPkgs()
{
	m_vecPkgs = m_adb.GetAllInstalledPackages(GetCurrDevice().wstrName);
	RecalcIndexes();
}

void CAPMDlg::OnBtnUninstPkgs()
{
	if (MessageBoxW(L"All selected packages will be uninstalled from your device.\r\nAre you sure?",
		L"Uninstallation warning!", MB_ICONEXCLAMATION | MB_YESNO) == IDNO) {
		return;
	}

	int iItem { -1 };
	for (auto i = 0UL; i < m_list.GetSelectedCount(); ++i) {
		iItem = m_list.GetNextItem(iItem, LVNI_SELECTED);
		m_adb.UninstallPkg(GetCurrDevice().wstrName, m_vecPkgs[m_vecPkgsIndexes[iItem]]);
	}

	::Sleep(300); //Delay to correctly get renewed packages list from a device.
	OnBtnListPkgs();
}

void CAPMDlg::OnComboDevicesSelChange()
{
	m_list.SetItemCountEx(0);
	m_vecPkgs.clear();
}

void CAPMDlg::OnDestroy()
{
	const auto fCloseServer = IsDlgButtonChecked(IDC_CHK_CLOSESRVONEXIT) == BST_CHECKED;
	if (fCloseServer) {
		m_adb.KillServer();
	}

	AfxGetApp()->WriteProfileInt(L"", L"close-adb-server-on-exit", static_cast<int>(fCloseServer));

	CDialogEx::OnDestroy();
}

void CAPMDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT pDIS)
{
	if (nIDCtl == IDC_LIST) {
		m_list.DrawItem(pDIS);
		return;
	}

	CDialogEx::OnDrawItem(nIDCtl, pDIS);
}

void CAPMDlg::OnDeviceListUpdated()
{
	OnComboDevicesSelChange();
	GetDlgItem(IDC_BTN_PKGS_LIST)->EnableWindow(m_cmbDevices.GetCount() > 0);
}

void CAPMDlg::OnEditFilterChange() {
	RecalcIndexes();
	m_list.SetItemState(-1, 0, LVIS_SELECTED); //Unselect all.
}

void CAPMDlg::OnListColumnClick(NMHDR *pNMHDR, LRESULT* /*pResult*/)
{
	std::ranges::sort(m_vecPkgs,
		[fAscending = m_list.GetSortAscending()](const std::wstring& wstrLHS, const std::wstring& wstrRHS) {
			const auto iCompare = wstrLHS.compare(wstrRHS);
			return fAscending ? iCompare < 0 : iCompare > 0;
	});

	RecalcIndexes();
}

void CAPMDlg::OnListGetDispInfo(NMHDR* pNMHDR, LRESULT* /*pResult*/)
{
	const auto di = reinterpret_cast<NMLVDISPINFOW*>(pNMHDR);
	const auto pItem = &di->item;
	const auto iItem = pItem->iItem;
	pItem->pszText = m_vecPkgs[m_vecPkgsIndexes[static_cast<size_t>(iItem)]].data();
}

void CAPMDlg::OnListItemChanged(NMHDR* pNMHDR, LRESULT* /*pResult*/)
{
	const auto uSelected = m_list.GetSelectedCount();
	GetDlgItem(IDC_BTN_PKGS_UNINSTALL)->EnableWindow(uSelected > 0);
	GetDlgItem(IDC_STATIC_PKGS_SELECTED)->SetWindowTextW(std::format(L"Selected: {}", uSelected).data());
}

void CAPMDlg::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT pMIS)
{
	if (nIDCtl == IDC_LIST) {
		m_list.MeasureItem(pMIS);
		return;
	}

	CDialogEx::OnMeasureItem(nIDCtl, pMIS);
}

void CAPMDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else {
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

auto CAPMDlg::GetCurrDevice()->const ADB::ADBDEVICE& {
	return m_vecDevices.at(m_cmbDevices.GetItemData(m_cmbDevices.GetCurSel()));
}

void CAPMDlg::RecalcIndexes()
{
	m_vecPkgsIndexes.clear();
	CStringW cwstr;
	GetDlgItem(IDC_EDIT_FILTER)->GetWindowTextW(cwstr);
	for (const auto& [index, wstr] : std::views::enumerate(m_vecPkgs)) {
		if (wstr.contains(cwstr)) {
			m_vecPkgsIndexes.emplace_back(index);
		}
	}

	m_list.SetItemCountEx(static_cast<int>(m_vecPkgsIndexes.size()));
	m_list.RedrawWindow();
	GetDlgItem(IDC_STATIC_PKGS)->SetWindowTextW(std::format(L"Packages: {}", m_vecPkgsIndexes.size()).data());
}