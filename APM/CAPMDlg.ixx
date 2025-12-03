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
	void FillComboOperType();
	[[nodiscard]] auto GetCurrDevice()const -> const ADB::ADBDEVICE&;
	[[nodiscard]] auto GetCurrPkgsType()const -> ADB::EPkgsType;
	[[nodiscard]] auto GetCurrOperType()const -> ADB::EOperType;
	BOOL OnInitDialog()override;
	afx_msg void OnBtnDevices();
	afx_msg void OnBtnInstallAPK();
	afx_msg void OnBtnPkgsShow();
	afx_msg void OnBtnPkgsOperate();
	afx_msg void OnComboDevicesSelChange();
	afx_msg void OnComboPkgsTypesSelChange();
	afx_msg void OnComboOperTypeSelChange();
	afx_msg void OnDestroy();
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT pDIS);
	afx_msg void OnEditFilterChange();
	afx_msg void OnListColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT pMIS);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	void RecalcIndexes();
	void SetStateBtnOperate();
	DECLARE_MESSAGE_MAP()
private:
	struct LISTITEM {
		std::wstring wstrData;
		bool fSelected { };
	};
	LISTEX::CListEx m_list;
	ADB m_adb;
	std::vector<ADB::ADBDEVICE> m_vecDevices;
	std::vector<LISTITEM> m_vecListItems;
	std::vector<std::size_t> m_vecFilteredIdxs;
	CComboBox m_cmbDevices;
	CComboBox m_cmbPkgsType;
	CComboBox m_cmbOperType;
};

BEGIN_MESSAGE_MAP(CAPMDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_DEVICES, &CAPMDlg::OnBtnDevices)
	ON_BN_CLICKED(IDC_BTN_INSTALL, &CAPMDlg::OnBtnInstallAPK)
	ON_BN_CLICKED(IDC_BTN_PKGSSHOW, &CAPMDlg::OnBtnPkgsShow)
	ON_BN_CLICKED(IDC_BTN_OPERATE, &CAPMDlg::OnBtnPkgsOperate)
	ON_CBN_SELCHANGE(IDC_CMB_DEVICES, &CAPMDlg::OnComboDevicesSelChange)
	ON_CBN_SELCHANGE(IDC_CMB_PKGSTYPE, &CAPMDlg::OnComboPkgsTypesSelChange)
	ON_CBN_SELCHANGE(IDC_CMB_OPERTYPE, &CAPMDlg::OnComboOperTypeSelChange)
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
	DDX_Control(pDX, IDC_CMB_PKGSTYPE, m_cmbPkgsType);
	DDX_Control(pDX, IDC_CMB_OPERTYPE, m_cmbOperType);
}

void CAPMDlg::FillComboOperType()
{
	using enum ADB::EPkgsType;
	using enum ADB::EOperType;

	m_cmbOperType.ResetContent();
	int index;

	switch (GetCurrPkgsType()) {
	case PKGS_ENABLED:
	case PKGS_SYSTEM:
	case PKGS_THRDPARTY:
		index = m_cmbOperType.AddString(L"Disable");
		m_cmbOperType.SetItemData(index, std::to_underlying(OPER_DISABLE));
		index = m_cmbOperType.AddString(L"Uninstall");
		m_cmbOperType.SetItemData(index, std::to_underlying(OPER_UNINSTALL));
		break;
	case PKGS_DISABLED:
		index = m_cmbOperType.AddString(L"Enable");
		m_cmbOperType.SetItemData(index, std::to_underlying(OPER_ENABLE));
		index = m_cmbOperType.AddString(L"Uninstall");
		m_cmbOperType.SetItemData(index, std::to_underlying(OPER_UNINSTALL));
		break;
	case PKGS_UNINSTALLED:
		index = m_cmbOperType.AddString(L"Restore");
		m_cmbOperType.SetItemData(index, std::to_underlying(OPER_RESTORE));
		break;
	default:
		break;
	}
	m_cmbOperType.SetCurSel(0);

	OnComboOperTypeSelChange();
}

auto CAPMDlg::GetCurrDevice()const->const ADB::ADBDEVICE& {
	return m_vecDevices.at(m_cmbDevices.GetItemData(m_cmbDevices.GetCurSel()));
}

auto CAPMDlg::GetCurrPkgsType()const->ADB::EPkgsType {
	return static_cast<ADB::EPkgsType>(m_cmbPkgsType.GetItemData(m_cmbPkgsType.GetCurSel()));
}

auto CAPMDlg::GetCurrOperType()const->ADB::EOperType {
	return static_cast<ADB::EOperType>(m_cmbOperType.GetItemData(m_cmbOperType.GetCurSel()));
}

BOOL CAPMDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_list.Create({ .hWndParent { m_hWnd }, .uID { IDC_LIST }, .fDialogCtrl { true }, .fSortable { true } });
	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	m_list.InsertColumn(0, L"Packages", 0, 700);

	using enum ADB::EPkgsType;
	auto index = m_cmbPkgsType.AddString(L"All enabled");
	m_cmbPkgsType.SetCurSel(index);
	m_cmbPkgsType.SetItemData(index, std::to_underlying(PKGS_ENABLED));
	index = m_cmbPkgsType.AddString(L"Disabled");
	m_cmbPkgsType.SetItemData(index, std::to_underlying(PKGS_DISABLED));
	index = m_cmbPkgsType.AddString(L"System");
	m_cmbPkgsType.SetItemData(index, std::to_underlying(PKGS_SYSTEM));
	index = m_cmbPkgsType.AddString(L"Third-party");
	m_cmbPkgsType.SetItemData(index, std::to_underlying(PKGS_THRDPARTY));
	index = m_cmbPkgsType.AddString(L"Uninstalled");
	m_cmbPkgsType.SetItemData(index, std::to_underlying(PKGS_UNINSTALLED));

	const auto uCloseServer = AfxGetApp()->GetProfileIntW(L"", L"close-adb-server-on-exit", 0);
	CheckDlgButton(IDC_CHK_CLOSESRVONEXIT, uCloseServer > 0 ? BST_CHECKED : BST_UNCHECKED);
	FillComboOperType();

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
	m_vecDevices.clear();
	m_vecDevices = m_adb.GetDevices();

	if (!m_vecDevices.empty()) {
		for (const auto& [index, dev] : std::views::enumerate(m_vecDevices)) {
			const auto wstr = std::format(L"{} ({})", dev.wstrModel, dev.wstrName);
			m_cmbDevices.SetItemData(m_cmbDevices.AddString(wstr.data()), index);
		}

		m_cmbDevices.SetCurSel(0);
	}

	OnComboDevicesSelChange();
	GetDlgItem(IDC_BTN_PKGSSHOW)->EnableWindow(m_cmbDevices.GetCount() > 0);
	GetDlgItem(IDC_BTN_INSTALL)->EnableWindow(m_cmbDevices.GetCount() > 0);
	pBtnDevs->EnableWindow(TRUE);
}

void CAPMDlg::OnBtnInstallAPK()
{
	IFileOpenDialog *pIFOD { };
	if (::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIFOD)) != S_OK) {
		return;
	}

	DWORD dwFlags;
	pIFOD->GetOptions(&dwFlags);
	pIFOD->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT | FOS_ALLOWMULTISELECT
		| FOS_DONTADDTORECENT | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
	COMDLG_FILTERSPEC arrFilter[] { { .pszName { L"APK (*.apk)" }, .pszSpec { L"*.apk" } },
		{ .pszName { L"All files (*.*)" }, .pszSpec { L"*.*" } } };
	pIFOD->SetFileTypes(2, arrFilter);

	if (pIFOD->Show(m_hWnd) != S_OK) //Cancel was pressed.
		return;

	IShellItemArray* pResults { };
	pIFOD->GetResults(&pResults);
	if (pResults == nullptr) {
		return;
	}

	DWORD dwCount { };
	pResults->GetCount(&dwCount);
	for (auto itFile { 0U }; itFile < dwCount; ++itFile) {
		IShellItem* pItem { };
		pResults->GetItemAt(itFile, &pItem);
		if (pItem == nullptr) {
			return;
		}

		wchar_t* pwszPath;
		pItem->GetDisplayName(SIGDN_FILESYSPATH, &pwszPath);
		const std::wstring wstrAPK = L"\"" + std::wstring(pwszPath) + L"\""; //Put path in quotes.
		m_adb.PkgOperate(GetCurrDevice().wstrName, wstrAPK, ADB::EOperType::OPER_INSTALL);
		pItem->Release();
		::CoTaskMemFree(pwszPath);
	}

	pResults->Release();
	pIFOD->Release();

	::Sleep(300); //Delay to correctly get renewed packages list from a device.
	OnBtnPkgsShow();
}

void CAPMDlg::OnBtnPkgsShow()
{
	if (m_vecDevices.empty())
		return;

	m_vecListItems.clear();

	for (auto& wstr : m_adb.GetPackagesList(GetCurrDevice().wstrName, GetCurrPkgsType())) {
		m_vecListItems.emplace_back(std::move(wstr), false);
	}

	RecalcIndexes();
}

void CAPMDlg::OnBtnPkgsOperate()
{
	using enum ADB::EOperType;
	const auto eOperType = GetCurrOperType();
	if (eOperType == OPER_UNINSTALL || eOperType == OPER_DISABLE) {
		std::wstring_view wsvMsg;
		switch (eOperType) {
		case OPER_UNINSTALL:
			wsvMsg = L"All selected packages will be uninstalled from your device.\r\nAre you sure?";
			break;
		case OPER_DISABLE:
			wsvMsg = L"All selected packages will be disabled on your device.\r\nAre you sure?";
			break;
		default:
			break;
		}

		if (MessageBoxW(wsvMsg.data(), L"Warning!", MB_ICONINFORMATION | MB_YESNO) == IDNO) {
			return;
		}

	}

	int iItem { -1 };
	for (auto i { 0UL }; i < m_list.GetSelectedCount(); ++i) {
		iItem = m_list.GetNextItem(iItem, LVNI_SELECTED);
		m_adb.PkgOperate(GetCurrDevice().wstrName, m_vecListItems[m_vecFilteredIdxs[iItem]].wstrData, eOperType);
	}
	m_list.SetItemState(-1, 0, LVIS_SELECTED); //Unselect all.
	m_list.RedrawWindow();

	::Sleep(300); //Delay to correctly get renewed packages list from a device.
	OnBtnPkgsShow();
}

void CAPMDlg::OnComboDevicesSelChange()
{
	m_list.SetItemCountEx(0);
	m_vecListItems.clear();
}

void CAPMDlg::OnComboPkgsTypesSelChange()
{
	FillComboOperType();
	OnBtnPkgsShow();
}

void CAPMDlg::OnComboOperTypeSelChange()
{
	using enum ADB::EOperType;
	std::wstring_view wsvName;

	switch (GetCurrOperType()) {
	case OPER_ENABLE:
		wsvName = L"Enable";
		break;
	case OPER_DISABLE:
		wsvName = L"Disable";
		break;
	case OPER_UNINSTALL:
		wsvName = L"Uninstall";
		break;
	case OPER_RESTORE:
		wsvName = L"Restore";
		break;
	default:
		break;
	}

	GetDlgItem(IDC_BTN_OPERATE)->SetWindowTextW(wsvName.data());
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

void CAPMDlg::OnEditFilterChange() {
	m_list.SetItemState(-1, 0, LVIS_SELECTED); //Unselect all.
	RecalcIndexes();
}

void CAPMDlg::OnListColumnClick(NMHDR *pNMHDR, LRESULT* /*pResult*/)
{
	int iItem { -1 };
	for (auto i { 0UL }; i < m_list.GetSelectedCount(); ++i) {
		iItem = m_list.GetNextItem(iItem, LVNI_SELECTED);
		m_list.SetItemData(iItem, 1);
		m_vecListItems[m_vecFilteredIdxs[static_cast<size_t>(iItem)]].fSelected = true;
	}
	m_list.SetItemState(-1, 0, LVIS_SELECTED); //Unselect all.

	std::ranges::sort(m_vecListItems, [fAscending = m_list.GetSortAscending()]
	(const LISTITEM& lhs, const LISTITEM& rhs) {
		const auto iCompare = lhs.wstrData.compare(rhs.wstrData);
		return fAscending ? iCompare < 0 : iCompare > 0;
	});

	RecalcIndexes();
	for (const auto& [index, itemIndex] : std::views::enumerate(m_vecFilteredIdxs)) {
		if (m_vecListItems[itemIndex].fSelected) {
			m_list.SetItemState(static_cast<int>(index), LVIS_SELECTED, LVIS_SELECTED);
			m_vecListItems[itemIndex].fSelected = false;
		}
	}
}

void CAPMDlg::OnListGetDispInfo(NMHDR* pNMHDR, LRESULT* /*pResult*/)
{
	const auto di = reinterpret_cast<NMLVDISPINFOW*>(pNMHDR);
	const auto pItem = &di->item;
	const auto iItem = pItem->iItem;
	pItem->pszText = m_vecListItems[m_vecFilteredIdxs[static_cast<size_t>(iItem)]].wstrData.data();
}

void CAPMDlg::OnListItemChanged(NMHDR* pNMHDR, LRESULT* /*pResult*/) {
	SetStateBtnOperate();
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

void CAPMDlg::RecalcIndexes()
{
	m_vecFilteredIdxs.clear();
	CStringW cwstr;
	GetDlgItem(IDC_EDIT_FILTER)->GetWindowTextW(cwstr);
	for (const auto& [index, item] : std::views::enumerate(m_vecListItems)) {
		if (item.wstrData.contains(cwstr)) {
			m_vecFilteredIdxs.emplace_back(index);
		}
	}

	m_list.SetItemCountEx(static_cast<int>(m_vecFilteredIdxs.size()));
	m_list.RedrawWindow();
	GetDlgItem(IDC_STATIC_ITEMS_TOTAL)->SetWindowTextW(std::format(L"Total items: {}", m_vecFilteredIdxs.size()).data());
}

void CAPMDlg::SetStateBtnOperate()
{
	const auto uSelected = m_list.GetSelectedCount();
	GetDlgItem(IDC_BTN_OPERATE)->EnableWindow(uSelected > 0);
	GetDlgItem(IDC_STATIC_ITEMS_SELECTED)->SetWindowTextW(std::format(L"Selected: {}", uSelected).data());
}