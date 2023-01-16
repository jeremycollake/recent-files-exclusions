// TODO: consider name change to recent items pruner
#include <iostream>
#include <thread>
#include <vector>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <functional>
#include <Windows.h>
#include <shlobj.h>
#include <conio.h>
#include "../libcommon/libcommon/libCommon.h"
#include "DebugOut.h"
#include "RecentItemsExclusions.h"
#include "PruningThread.h"
#include "ListSerializer.h"
#include "ListDialog.h"
#include "UpdateCheckFuncs.h"
#include "resource.h"

RecentItemsExclusions g_RecentItemsExclusionsApp;	// app globals and config
PruningThread g_PruningThread;

DWORD WINAPI UpdateCheckThread(LPVOID lpv)
{
	//
	// TODO code this thread
	// g_RecentItemsExclusionsApp.hExitEvent;
	//	
}

bool CreateOrReinitializeTrayWindow(const bool bFirstTimeCreation)
{
	if (!bFirstTimeCreation)
	{
		NOTIFYICONDATA ndata = {};
		memset(&ndata, 0, sizeof(NOTIFYICONDATA)); // redundant
		ndata.cbSize = sizeof(NOTIFYICONDATA);
		ndata.hWnd = g_RecentItemsExclusionsApp.hWndSysTray;
		ndata.uID = GetCurrentProcessId();	// our tray window ID will be our PID
		ndata.uCallbackMessage = RecentItemsExclusions::UWM_TRAY;
		Shell_NotifyIcon(NIM_DELETE, &ndata);

		SendMessage(g_RecentItemsExclusionsApp.hWndSysTray, RecentItemsExclusions::UWM_REGISTER_TRAY_ICON, 0, 0);
	}
	else
	{
		_ASSERT(!g_RecentItemsExclusionsApp.hWndSysTray);

		WNDCLASSEX wcex = {};
		wcex.cbSize = sizeof(wcex);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = (WNDPROC)TrayWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = g_RecentItemsExclusionsApp.hInst;
		wcex.hIcon = LoadIcon(g_RecentItemsExclusionsApp.hResourceModule, (LPCTSTR)IDI_ICON1);
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = (LPCTSTR)IDR_MENU_TRAY;
		wcex.lpszClassName = g_RecentItemsExclusionsApp.SYSTRAY_WINDOW_CLASS_NAME;
		wcex.hIconSm = wcex.hIcon;
		RegisterClassEx(&wcex);

		g_RecentItemsExclusionsApp.hWndSysTray = CreateWindow(g_RecentItemsExclusionsApp.SYSTRAY_WINDOW_CLASS_NAME, g_RecentItemsExclusionsApp.SYSTRAY_WINDOW_NAME,
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, g_RecentItemsExclusionsApp.hInst, NULL);
		_ASSERT(g_RecentItemsExclusionsApp.hWndSysTray);
		if (g_RecentItemsExclusionsApp.hWndSysTray)
		{
			SendMessage(g_RecentItemsExclusionsApp.hWndSysTray, RecentItemsExclusions::UWM_REGISTER_TRAY_ICON, 0, 0);
		}
	}
	return true;
}

LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	static HICON s_hAppIcon = NULL;
	static UINT s_uTaskbarRestartMessageId = 0;
	switch (nMessage)
	{
	case WM_CREATE:
		// register to get taskbar recreation messages
		s_uTaskbarRestartMessageId = RegisterWindowMessage(L"TaskbarCreated");
		s_hAppIcon = LoadIcon(g_RecentItemsExclusionsApp.hInst, MAKEINTRESOURCE(IDI_ICON1));
		PostMessage(hWnd, RecentItemsExclusions::UWM_START_PRUNING_THREAD, 0, 0);
		return 0;
	case WM_DESTROY:
	{
		DEBUG_PRINT(L"WM_DESTROY");
		NOTIFYICONDATA ndata = {};
		memset(&ndata, 0, sizeof(NOTIFYICONDATA)); // redundant
		ndata.cbSize = sizeof(NOTIFYICONDATA);
		ndata.hWnd = g_RecentItemsExclusionsApp.hWndSysTray;
		ndata.uID = GetCurrentProcessId();	// our tray window ID will be our PID
		ndata.uCallbackMessage = RecentItemsExclusions::UWM_TRAY;
		Shell_NotifyIcon(NIM_DELETE, &ndata);
		return 0;
	}
	case RecentItemsExclusions::UWM_START_PRUNING_THREAD:
	{
		g_PruningThread.Stop();		// ensure stopped
		std::vector<std::wstring> vMatchPhrases;
		// temp testing
		vMatchPhrases.push_back(L"prI");
		g_PruningThread.Start(vMatchPhrases);
		return 0;
	}
	case RecentItemsExclusions::UWM_STOP_PRUNING_THREAD:
		g_PruningThread.Stop();
		return 0;
	case RecentItemsExclusions::UWM_REGISTER_TRAY_ICON:
	{
		DEBUG_PRINT(L"UWM_REGISTER_TRAY_ICON");
		_ASSERT(s_hAppIcon);
		NOTIFYICONDATA ndata = {};
		memset(&ndata, 0, sizeof(ndata));	// redundant
		ndata.cbSize = sizeof(NOTIFYICONDATA);
		ndata.hWnd = hWnd;
		ndata.uID = GetCurrentProcessId();	// our tray window ID will be our PID
		ndata.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
		wcsncpy_s(ndata.szTip, _countof(ndata.szTip), PRODUCT_NAME, _TRUNCATE);
		ndata.hIcon = s_hAppIcon;
		ndata.uTimeout = 10000; // just to have a default timeout
		ndata.uCallbackMessage = RecentItemsExclusions::UWM_TRAY;

		// set version afterwards			
		DEBUG_PRINT(L"NIM_ADD");
		if (!Shell_NotifyIcon(NIM_ADD, &ndata))
		{
			// TODO: this is probably not necessary, but done in older projects, so we'll continue since the consequence is the app not running at startup
			//  theory is that explorer could be initializing, or reinitializing after our app starts.
			//	Shouldn't occur to due TaskbarCreated notification coming reliably after explorer (re)starts
			for (int n = 0; n < 20; n++)
			{
				DEBUG_PRINT(L"NIM_ADD failed. retry %d", n);
				Sleep(100);
				if (Shell_NotifyIcon(NIM_ADD, &ndata))
				{
					break;
				}
			}
		}
		// assume NIM_ADD or a failsafe retry succeeded
		ndata.uVersion = NOTIFYICON_VERSION;
		Shell_NotifyIcon(NIM_SETVERSION, &ndata);
	}
	break;
	case RecentItemsExclusions::UWM_TRAY:
		switch (lParam)
		{
		case NIN_BALLOONHIDE:
		case NIN_BALLOONTIMEOUT:
			break;
		case NIN_BALLOONUSERCLICK:
			break;
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
			if (DialogBoxParam(g_RecentItemsExclusionsApp.hInst, MAKEINTRESOURCE(IDD_EXCLUSIONS_LIST), NULL, &ListDialogProc, 0) == 0)
			{
				// reload list and restart thread
				// TODO: create watcher thread on config list in case externally changed
				std::vector<std::wstring> vStrings;
				g_RecentItemsExclusionsApp.ListSerializer.LoadListFromFile(g_RecentItemsExclusionsApp.g_strListSavePath, vStrings);
				SendMessage(hWnd, RecentItemsExclusions::UWM_START_PRUNING_THREAD, 0, 0);
			}
			break;
		case WM_RBUTTONUP:
		{
			POINT pt = {};
			GetCursorPos(&pt);

			//
			// make sure latest settings are loaded!
			//
			// unknown crash seen here in debugger
			//g_pMainWindow->m_cSettings.LoadConfigurationFromFile();

			static HMENU s_hTrayMenu = NULL;
			HMENU hMenuPopup = NULL;
			if (!s_hTrayMenu)
			{
				s_hTrayMenu = LoadMenu(g_RecentItemsExclusionsApp.hInst, MAKEINTRESOURCE(IDR_MENU_TRAY));
			}
			hMenuPopup = GetSubMenu(s_hTrayMenu, 0);

			// this is MANDATORY - do not remove
			SetForegroundWindow(hWnd);

			unsigned long  nTrackRes = (unsigned long)TrackPopupMenu(hMenuPopup,
				TPM_RETURNCMD |
				TPM_RIGHTBUTTON,
				pt.x, pt.y,
				0,
				hWnd,
				NULL);

			switch (nTrackRes)
			{
			case ID_TRAY_OPEN:
				SendMessage(hWnd, RecentItemsExclusions::UWM_TRAY, 0, WM_LBUTTONUP);
				break;
			case ID_TRAY_EXIT:
				PostQuitMessage(0);
				break;
			default:
				break;
			}
		} // end WM_RBUTTONUP
		break;
		} // end UWM_SYSTRAY
	default:
		if (nMessage == s_uTaskbarRestartMessageId)
		{
			DEBUG_PRINT(L"TaskbarCreateMessage");
			CreateOrReinitializeTrayWindow(false);
		}
		else
		{
			return DefWindowProc(hWnd, nMessage, wParam, lParam);
		}
	}
	return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	g_RecentItemsExclusionsApp.hInst = g_RecentItemsExclusionsApp.hResourceModule = GetModuleHandle(nullptr);

	// initialize common controls
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(icex);
	icex.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_LINK_CLASS;
	if (!InitCommonControlsEx(&icex))
	{
		// W2K and XP pre-SP2
		InitCommonControls();
	}

	CreateOrReinitializeTrayWindow(true);

	// returns non-zero if message is OTHER than WM_QUIT, or -1 if invalid window handle
	// see https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessage (recommends this exact clause)
	BOOL bRet;
	MSG msg;
	HACCEL hAccelTable = LoadAccelerators(g_RecentItemsExclusionsApp.hInst, (LPCTSTR)IDR_ACCELERATOR1);

	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			// handle the error and possibly exit
		}
		else
		{
			if (!TranslateAccelerator(g_RecentItemsExclusionsApp.hWndSysTray, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	return 0;
}