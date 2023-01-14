#pragma once

#include "../libcommon/libcommon/libCommon.h"
#include "ListSerializer.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

class RecentItemsExclusions
{
public:
	RecentItemsExclusions()
	{
		// build up list config save path incrementally to ensure parent folders exist
		g_strListSavePath = GetAppDataPath();
		CreateDirectory(g_strListSavePath.c_str(), NULL);
		g_strListSavePath += L"\\RecentItemsExclusions";
		CreateDirectory(g_strListSavePath.c_str(), NULL);
		g_strListSavePath += L"\\config";
		CreateDirectory(g_strListSavePath.c_str(), NULL);
		g_strListSavePath += L"\\RecentItemsExclusions.txt";
	}
	ListSerializer ListSerializer;
	std::wstring g_strAppName = L"Recent Items Pruner";
	std::wstring g_strListSavePath;
	const WCHAR* SYSTRAY_WINDOW_CLASS_NAME = L"RecentItemsExclusions_TrayClass";
	const WCHAR* SYSTRAY_WINDOW_NAME = L"RecentItemsExclusions_TrayWnd";
	const static UINT UWM_TRAY = WM_USER + 1;
	const static UINT UWM_REGISTER_TRAY_ICON = WM_USER + 2;
	const static UINT UWM_START_PRUNING_THREAD = WM_USER + 3;
	const static UINT UWM_STOP_PRUNING_THREAD = WM_USER + 4;

	HINSTANCE g_hInst = NULL;
	HWND g_hWndSysTray = NULL;
};

bool CreateOrReinitializeTrayWindow(const bool bFirstTimeCreation = true);
LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam);