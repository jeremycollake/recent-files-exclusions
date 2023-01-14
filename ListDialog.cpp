#include "ListDialog.h"
#include <windowsx.h>
#include "DebugOut.h"
#include "resource.h"

extern std::wstring g_strAppName;

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
		s_bChangesMade = false;	// reinit for subsequent dialog instances
		//SetStringsInDialog(GetDlgItem(hDlg, IDC_LIST), pDialogInfo->m_pvsExclusionStrings);		
		return TRUE;
	case WM_CLOSE:
		if (s_bChangesMade)
		{
			if (MessageBox(hDlg, L"Save changes?", g_strAppName.c_str(), MB_ICONQUESTION | MB_YESNO) == IDYES)
			{
				PostMessage(hDlg, WM_COMMAND, IDOK, 0);
				break;
			}
		}
		EndDialog(hDlg, 1);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_CLEAR:
			ListBox_ResetContent(GetDlgItem(hDlg, IDC_LIST_STRINGS));
			return TRUE;
		case IDC_ADD:
			/*
			pDialogInfo = (SIMPLE_STRING_EXCLUSION_DIALOG_INFO*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
			if (pDialogInfo->m_bProRestrictAddButton)
			{
				g_cLicensing.ShowProOnlyFeatureNotice(hDlg);
				return TRUE;
			}			
			hWndList = GetDlgItem(hDlg, IDC_LIST1);
			GetDlgItemText(hDlg, IDC_EDIT_NEW_EXCLUSION, tszT, _countof(tszT) - 1);
			if (_tcslen(tszT))
			{
				SendMessage(hWndList, LB_ADDSTRING, 0, (LPARAM)&tszT);
			}
			SetDlgItemText(hDlg, IDC_EDIT_NEW_EXCLUSION, NULL);
			pDialogInfo->m_bChangesMade = true;
			*/
			return TRUE;
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
			// save new list

			EndDialog(hDlg, 0);
			return TRUE;
		}
		case IDCANCEL:
			if (s_bChangesMade)
			{
				if (MessageBox(hDlg, L"Save changes?", g_strAppName.c_str(), MB_ICONQUESTION | MB_YESNO) == IDYES)
				{
					PostMessage(hDlg, WM_COMMAND, IDOK, 0);
					break;
				}
			}
			EndDialog(hDlg, 1);
			return TRUE;
		}
		break;
	}
	return FALSE;
}
