#include "AboutDialog.h"
#include "versioninfo.h"
#include "resource.h"

INT_PTR WINAPI AboutDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetWindowText(hDlg, std::wstring(L"About " PRODUCT_NAME).c_str());
		SetDlgItemText(hDlg, IDC_VERSIONINFO, std::wstring(PRODUCT_NAME L" v" PRODUCT_VERSION "\n" PRODUCT_COPYRIGHT).c_str());
		return TRUE;
	}
	break;
	case WM_CLOSE:
	{
		EndDialog(hDlg, 0);
		return TRUE;
	}
	break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			SendMessage(hDlg, WM_CLOSE, 0, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}