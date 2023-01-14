#pragma once

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace RecentItemsExclusions
{
	const static WCHAR* SYSTRAY_WINDOW_CLASS_NAME = L"RecentItemsExclusions_TrayClass";
	const static WCHAR* SYSTRAY_WINDOW_NAME = L"RecentItemsExclusions_TrayWnd";
	const UINT UWM_TRAY = WM_USER + 1;
	const UINT UWM_REGISTER_TRAY_ICON = WM_USER + 2;
	const UINT UWM_START_PRUNING_THREAD = WM_USER + 3;
	const UINT UWM_STOP_PRUNING_THREAD = WM_USER + 4;
};

bool CreateOrReinitializeTrayWindow(const bool bFirstTimeCreation = true);
LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam);