#include "ListDialog.h"
#include <windowsx.h>
#include "RecentItemsExclusions.h"
#include "UpdateCheckFuncs.h"
#include "AboutDialog.h"
#include "DebugOut.h"
#include "resource.h"

extern RecentItemsExclusions g_RecentItemsExclusionsApp;

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
	TCHAR wszT[1024] = { 0 };

	switch (message)
	{
	case WM_INITDIALOG:
	{
		g_RecentItemsExclusionsApp.hWndListDialog = hDlg;

		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadImage(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, 0));
		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, 0));

		s_bChangesMade = false;	// reinit for subsequent dialog instances

		SetMenu(hDlg, LoadMenu(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDR_MENU_MAIN)));

		std::vector<std::wstring> vStrings;
		if (g_RecentItemsExclusionsApp.ListSerializer.LoadListFromFile(g_RecentItemsExclusionsApp.strListSavePath, vStrings) > 0)
		{
			SetListInDialog(GetDlgItem(hDlg, IDC_LIST_STRINGS), vStrings);
		}

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
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_HELP_ABOUT:
		{
			DialogBoxParam(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDD_ABOUT), NULL, &AboutDialogProc, 0);
			return TRUE;
		}
		break;
		case ID_HELP_CHECKFORUPDATES:
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
						DownloadAndApplyUpdate();
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
		case IDC_CLEAR:
			ListBox_ResetContent(GetDlgItem(hDlg, IDC_LIST_STRINGS));
			return TRUE;
		case IDC_ADD:
		{
			std::wstring wstrNew;
			if (DialogBoxParam(g_RecentItemsExclusionsApp.hResourceModule, MAKEINTRESOURCE(IDD_ENTRYINPUT), NULL, &NewEntryDialogProc, reinterpret_cast<LPARAM>(&wstrNew)) == 0)
			{
				if (wstrNew.length())
				{
					ListBox_AddString(GetDlgItem(hDlg, IDC_LIST_STRINGS), wstrNew.c_str());
					s_bChangesMade = true;
				}
			}
			return TRUE;
		}
		case IDC_REMOVE:
		{
			hWndList = GetDlgItem(hDlg, IDC_LIST_STRINGS);
			int nI = (int)SendMessage(hWndList, LB_GETCURSEL, 0, 0);
			SendMessage(hWndList, LB_GETTEXT, nI, (LPARAM)&wszT);
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
		SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)lParam);
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
			// read text from edit control
			WCHAR wszT[1024] = { 0 };
			Edit_GetText(GetDlgItem(hDlg, IDC_EDIT1), wszT, _countof(wszT) - 1);
			*pwstr = wszT;
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