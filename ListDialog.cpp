#include "ListDialog.h"
#include <windowsx.h>
#include "../libcommon/libcommon/win32-darkmode/win32-darkmode/DarkMode.h"
#include "RecentItemsExclusions.h"
#include "PruningThread.h"
#include "UpdateCheckFuncs.h"
#include "AboutDialog.h"
#include "DebugOut.h"
#include "resource.h"

extern RecentItemsExclusions g_RecentItemsExclusionsApp;
extern PruningThread g_PruningThread;						// primary work thread

bool SetListInDialog(const HWND hWndList, const std::vector<std::wstring>& vStrings)
{
	for (auto& i : vStrings)
	{
		ListBox_AddString(hWndList, i.c_str());
	}
	return true;
}

bool GetListFromDialog(HWND hWndList, std::vector<std::wstring>& vStrings)
{
	vStrings.clear();
	int nItems = ListBox_GetCount(hWndList);
	WCHAR wszT[1024] = { 0 };
	for (int nCurrentItem = 0; nCurrentItem < nItems; nCurrentItem++)
	{
		if (ListBox_GetTextLen(hWndList, nCurrentItem) >= _countof(wszT))
		{
			DEBUG_PRINT(L"ERROR: String too large.");
		}
		else
		{
			wszT[0] = 0;
			ListBox_GetText(hWndList, nCurrentItem, wszT);
			if (wcslen(wszT))
			{
				vStrings.push_back(wszT);
			}
			else
			{
				break;
			}
		}
	}
	return true;
}

INT_PTR WINAPI ListDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hWndList;
	static bool s_bChangesMade = false;
	static HBRUSH hbrBkgnd = nullptr;
	static const int ctlIds[] = { IDOK, IDCANCEL, IDC_RESET, IDC_STATS, IDC_ADD, IDC_REMOVE, IDC_CLEAR, IDC_LIST_STRINGS, IDC_STATIC_DESCRIPTION, IDC_STATIC_1, IDC_STATIC_2 };

	switch (message)
	{
	case WM_INITDIALOG:
	{
		g_RecentItemsExclusionsApp.hWndListDialog = hDlg;

		if (g_darkModeSupported && g_darkModeEnabled)
		{
			for (auto i : ctlIds)
			{
				SetWindowTheme(GetDlgItem(hDlg, i), L"Explorer", nullptr);
			}
			SendMessageW(hDlg, WM_THEMECHANGED, 0, 0);
		}

		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadImage(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, 0));
		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, 0));

		s_bChangesMade = false;	// reinit for subsequent dialog instances		

		SetMenu(hDlg, LoadMenu(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDR_MENU_MAIN)));

		SetDlgItemText(hDlg, IDC_STATIC_DESCRIPTION, ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_PRODUCT_DESCRIPTION).c_str());

		std::vector<std::wstring> vStrings;
		if (g_RecentItemsExclusionsApp.ListSerializer.LoadListFromFile(g_RecentItemsExclusionsApp.strListSavePath, vStrings) > 0)
		{
			SetListInDialog(GetDlgItem(hDlg, IDC_LIST_STRINGS), vStrings);
		}

		// register for notifications of pruning thread state change
		g_PruningThread.m_pruningStatus.AddListeningEvent(g_RecentItemsExclusionsApp.hPruningThreadStatusChangedEvent);

		// populate initial stats
		SendMessage(hDlg, RecentItemsExclusions::UWM_STATUS_CHANGED, 0, 0);

		return TRUE;
	}
	case WM_CLOSE:
		if (s_bChangesMade)
		{
			if (MessageBox(hDlg, L"Save changes?", PRODUCT_NAME, MB_ICONQUESTION | MB_YESNO) == IDYES)
			{
				PostMessage(hDlg, WM_COMMAND, IDOK, 0);
				break;
			}
		}
		EndDialog(hDlg, 1);
		return TRUE;
	case WM_DESTROY:
		// set HWND to NULL to indicate dialog is closed
		g_RecentItemsExclusionsApp.hWndListDialog = NULL;
		return 0;
	case WM_CTLCOLORBTN:	// unused by pushbuttons due to multiple colors being used
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORMSGBOX:
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORSCROLLBAR:
	{
		if (g_darkModeSupported && g_darkModeEnabled)
		{
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, g_RecentItemsExclusionsApp.DARK_WINDOW_TEXT_COLOR);
			SetBkColor(hdc, g_RecentItemsExclusionsApp.DARK_WINDOW_BACKGROUND);
			if (!hbrBkgnd)
			{
				hbrBkgnd = CreateSolidBrush(g_RecentItemsExclusionsApp.DARK_WINDOW_BACKGROUND);
			}
			return reinterpret_cast<INT_PTR>(hbrBkgnd);
		}
	}
	break;
	case WM_SETTINGCHANGE:
	{
		if (g_darkModeSupported && IsColorSchemeChangeMessage(lParam))
		{
			SendMessageW(hDlg, WM_THEMECHANGED, 0, 0);
		}
	}
	break;
	case WM_THEMECHANGED:
	{
		if (g_darkModeSupported)
		{
			_AllowDarkModeForWindow(hDlg, g_darkModeEnabled);
			RefreshTitleBarThemeColor(hDlg);

			for (auto i : ctlIds)
			{
				HWND hButton = GetDlgItem(hDlg, i);
				_AllowDarkModeForWindow(hButton, g_darkModeEnabled);
				SendMessageW(hButton, WM_THEMECHANGED, 0, 0);
			}

			UpdateWindow(hDlg);
		}
	}
	break;
	case WM_ENTERMENULOOP:
	{
		DEBUG_PRINT(L"WM_ENTERMENULOOP, updating menu item states...");
		auto hMenu = GetMenu(hDlg);
		if (hMenu)
		{
			// if beta version, don't allow toggle off of include betas. This is forced on in winmain.
#ifdef BETA_VERSION
			EnableMenuItem(hMenu, ID_UPDATES_INCLUDEBETAS, MF_GRAYED | MF_DISABLED);
#endif	
			CheckMenuItem(hMenu, ID_UPDATES_INCLUDEBETAS, g_RecentItemsExclusionsApp.AreBetaUpdatesEnabled() ? MF_CHECKED : MF_UNCHECKED);
			// hMenu doesn't need freed
		}
		break;
	}
	case RecentItemsExclusions::UWM_STATUS_CHANGED:
	{
		DEBUG_PRINT(L"UWM_STATUS_CHANGED");

		CString csStr;
		csStr.Format(L"Pruning thread state: %s\n"
			"\nItems Last Scanned: %u"
			"\nTotal Items Pruned: %u"
			"\nItems Pruned Today: %u",
			g_PruningThread.m_pruningStatus.GetStateString().c_str(),
			g_PruningThread.m_pruningStatus.GetItemsLastScannedCount(),
			g_PruningThread.m_pruningStatus.GetTotalItemsPrunedCount(),
			g_PruningThread.m_pruningStatus.GetItemsPrunedTodayCount());

		SetDlgItemText(hDlg, IDC_STATS, csStr);
	}
	break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_HELP_ABOUT:
		{
			DialogBoxParam(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDD_ABOUT), NULL, &AboutDialogProc, 0);
			return TRUE;
		}
		break;
		case ID_UPDATES_INCLUDEBETAS:
		{
			g_RecentItemsExclusionsApp.SetBetaUpdatesEnabled(!g_RecentItemsExclusionsApp.AreBetaUpdatesEnabled());
		}
		break;
		case ID_UPDATES_CHECKFORUPDATES:
		{
			std::wstring strLatestVer;
			FetchLatestVersionNumber(&strLatestVer, NULL);
			if (!strLatestVer.empty())
			{
				g_RecentItemsExclusionsApp.strFetechedVersionAvailableForDownload = strLatestVer;
				DEBUG_PRINT(L"Fetched latest version is %s", strLatestVer.c_str());
				// check if available version is newer than current, and if so, notify user
				if (TextVersionToULONG(strLatestVer.c_str()) > TextVersionToULONG(PRODUCT_VERSION))
				{
					if (MessageBox(hDlg, L"A newer version is available. Update now?", PRODUCT_NAME, MB_ICONINFORMATION | MB_YESNO) == IDYES)
					{
						if (!DownloadAndApplyUpdate())
						{
							MessageBox(hDlg, ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_UPDATE_ERROR_LAUNCHING).c_str(), PRODUCT_NAME, MB_ICONSTOP);
						}
					}
				}
				else
				{
					MessageBox(hDlg, L"No update available.", PRODUCT_NAME, MB_ICONINFORMATION);
				}
			}
			return TRUE;
		}
		break;
		case ID_FILE_MINIMIZE:
		{
			PostMessage(hDlg, WM_CLOSE, 0, 0);
			return TRUE;
		}
		break;
		case IDC_RESET:
		{
			g_PruningThread.m_pruningStatus.Reset();
			return TRUE;
		}
		break;
		case IDC_CLEAR:
		{
			ListBox_ResetContent(GetDlgItem(hDlg, IDC_LIST_STRINGS));
			return TRUE;
		}
		break;
		case IDC_ADD:
		{
			std::wstring wstrNew;
			if (DialogBoxParam(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDD_ENTRYINPUT), NULL, &NewEntryDialogProc, reinterpret_cast<LPARAM>(&wstrNew)) == 0)
			{
				if (wstrNew.length())
				{
					if (ListBox_FindString(GetDlgItem(hDlg, IDC_LIST_STRINGS), -1, wstrNew.c_str()) != LB_ERR)
					{
						MessageBox(hDlg, L"ERROR: Item already in list", PRODUCT_NAME, MB_ICONERROR);
					}
					else
					{
						ListBox_AddString(GetDlgItem(hDlg, IDC_LIST_STRINGS), wstrNew.c_str());
					}
					s_bChangesMade = true;
				}
			}
			return TRUE;
		}
		case IDC_REMOVE:
		{
			hWndList = GetDlgItem(hDlg, IDC_LIST_STRINGS);
			int nI = (int)SendMessage(hWndList, LB_GETCURSEL, 0, 0);
			SendMessage(hWndList, LB_DELETESTRING, nI, 0);
			s_bChangesMade = true;
			return TRUE;
		}
		case IDOK:
		{
			std::vector<std::wstring> vStrings;
			GetListFromDialog(GetDlgItem(hDlg, IDC_LIST_STRINGS), vStrings);
			g_RecentItemsExclusionsApp.ListSerializer.SaveListToFile(g_RecentItemsExclusionsApp.strListSavePath, vStrings);
			EndDialog(hDlg, 0);
			return TRUE;
		}
		case IDCANCEL:
		{
			if (s_bChangesMade)
			{
				if (MessageBox(hDlg, L"Save changes?", PRODUCT_NAME, MB_ICONQUESTION | MB_YESNO) == IDYES)
				{
					PostMessage(hDlg, WM_COMMAND, IDOK, 0);
					break;
				}
			}
			EndDialog(hDlg, 1);
			return TRUE;
		} // end IDCANCEL			
		} // end WM_COMMAND
		break;
	}
	return FALSE;
}

// lParam on create is std::wstring* to hold the returned string
INT_PTR WINAPI NewEntryDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		_ASSERT(lParam);
		if (!lParam)
		{
			return FALSE;
		}
		SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
		PostMessage(hDlg, WM_NEXTDLGCTL, IDC_EDIT1, TRUE);
		return TRUE;
	case WM_CLOSE:
		EndDialog(hDlg, 1);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		{
			auto pwstr = reinterpret_cast<std::wstring*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			_ASSERT(pwstr);
			WCHAR wszT[1024] = { 0 };
			Edit_GetText(GetDlgItem(hDlg, IDC_EDIT1), wszT, _countof(wszT) - 1);
			*pwstr = wszT;
			if (pwstr->find_first_of(L"*?") != std::wstring::npos)
			{
				pwstr->clear();
				MessageBox(hDlg, L"Wildcards are not supported. Use substrings instead. For instance, .pdf instead of *.pdf.", PRODUCT_NAME, MB_ICONINFORMATION);
				break;
			}
			EndDialog(hDlg, 0);
			return TRUE;
		}
		case IDCANCEL:
			SendMessage(hDlg, WM_CLOSE, 0, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}