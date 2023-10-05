#include <iostream>
#include <thread>
#include <vector>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <functional>
#include <boost/program_options.hpp>
#include <Windows.h>
#include <shlobj.h>
#include <conio.h>
#include "../libcommon/libcommon/libCommon.h"
#include "../libcommon/libcommon/win32-darkmode/win32-darkmode/darkmode.h"
#include "DebugOut.h"
#include "RecentItemsExclusions.h"
#include "PruningThread.h"
#include "ListSerializer.h"
#include "ListDialog.h"
#include "UpdateCheckFuncs.h"
#include "resource.h"

RecentItemsExclusions g_RecentItemsExclusionsApp;	// app globals and config
PruningThread g_PruningThread;						// primary work thread

namespace po = boost::program_options;

void ExitSignalWatchThread()
{
	DEBUG_PRINT(L"ExitSignalWatchThread");
	_ASSERT(g_RecentItemsExclusionsApp.hExitEvent && g_RecentItemsExclusionsApp.hExternalExitSignal);
	HANDLE hWaitObjects[2] = { g_RecentItemsExclusionsApp.hExitEvent, g_RecentItemsExclusionsApp.hExternalExitSignal };
	WaitForMultipleObjects(_countof(hWaitObjects), hWaitObjects, FALSE, INFINITE);
	DEBUG_PRINT(L"ExitSignalWatchThread Exit event received...");
	// ensure local exit event set in case we received global
	SetEvent(g_RecentItemsExclusionsApp.hExitEvent);
	if (g_RecentItemsExclusionsApp.hWndSysTray)
	{
		PostMessage(g_RecentItemsExclusionsApp.hWndSysTray, g_RecentItemsExclusionsApp.UWM_EXIT, 0, 0);
	}
	return;
}

void PruningThreadStatusWatchThread()
{
	DEBUG_PRINT(L"PruningThreadStatusWatchThread");
	_ASSERT(g_RecentItemsExclusionsApp.hExitEvent && g_RecentItemsExclusionsApp.hPruningThreadStatusChangedEvent);
	HANDLE hWaitObjects[2] = { g_RecentItemsExclusionsApp.hExitEvent, g_RecentItemsExclusionsApp.hPruningThreadStatusChangedEvent };
	bool bEnd = false;
	do
	{
		DWORD dwWaitResult = WaitForMultipleObjects(_countof(hWaitObjects), hWaitObjects, FALSE, INFINITE);
		switch (dwWaitResult)
		{
		case WAIT_OBJECT_0:
			DEBUG_PRINT(L"PruningThreadStatusWatchThread Exit event received...");
			bEnd = true;
			break;
		case WAIT_OBJECT_0 + 1:
			DEBUG_PRINT(L"PruningThreadStatusWatchThread Status changed event received...");
			// send notification message to dialog window, if open
			if (g_RecentItemsExclusionsApp.hWndListDialog)
			{
				PostMessage(g_RecentItemsExclusionsApp.hWndListDialog, g_RecentItemsExclusionsApp.UWM_STATUS_CHANGED, 0, 0);
			}
			break;
		default:
			DEBUG_PRINT(L"PruningThreadStatusWatchThread WaitForMultipleObjects failed");
			bEnd = true;
			break;
		}
	} while (!bEnd);

	DEBUG_PRINT(L"PruningThreadStatusWatchThread ends");
	return;
}

// UpdateCheckThread - periodically checks for updates
void UpdateCheckThread()
{
	DEBUG_PRINT(L"UpdateCheckThread");
	_ASSERT(g_RecentItemsExclusionsApp.hExitEvent);
	// do one check when thread starts
	bool bEnd = false;
	do
	{
		DEBUG_PRINT(L"UpdateCheckThread checking for update...");
		std::wstring strLatestVer;
		FetchLatestVersionNumber(&strLatestVer, NULL);
		if (!strLatestVer.empty())
		{
			g_RecentItemsExclusionsApp.strFetechedVersionAvailableForDownload = strLatestVer;
			DEBUG_PRINT(L"Fetched latest version is %s", strLatestVer.c_str());
			// check if available version is newer than current, and if so, notify user
			if (TextVersionToULONG(strLatestVer.c_str()) > TextVersionToULONG(PRODUCT_VERSION))
			{
				DEBUG_PRINT(L"New version available!");
				// notify user				
				PostMessage(g_RecentItemsExclusionsApp.hWndSysTray, RecentItemsExclusions::UWM_NEW_VERSION_AVAILABLE, 0, 0);
			}
		}
		DWORD dwWaitResult = WaitForSingleObject(g_RecentItemsExclusionsApp.hExitEvent, g_RecentItemsExclusionsApp.UPDATE_CHECK_INTERVAL_MS);
		switch (dwWaitResult)
		{
		case WAIT_OBJECT_0:
			DEBUG_PRINT(L"Update thread received exit signal");
			bEnd = true;
			break;
		case WAIT_TIMEOUT:
			DEBUG_PRINT(L"Update thread periodic check");
			break;
		default:
			DEBUG_PRINT(L"Update thread unexpected wait result, aborting");
			break;
		}
	} while (!bEnd);
	DEBUG_PRINT(L"UpdateCheckThread ends");
	return;
}

// returns true if an existing instance was found
bool BringExistingInstanceToForeground()
{
	HWND hWnd = FindWindow(g_RecentItemsExclusionsApp.SYSTRAY_WINDOW_CLASS_NAME, nullptr);
	if (hWnd)
	{
		// found existing instance, issue open main dialog command
		SendMessage(hWnd, RecentItemsExclusions::UWM_OPEN_LIST_DIALOG, 0, 0);
		return true;
	}
	return false;
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

		CreateWindow(g_RecentItemsExclusionsApp.SYSTRAY_WINDOW_CLASS_NAME, g_RecentItemsExclusionsApp.SYSTRAY_WINDOW_NAME,
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, g_RecentItemsExclusionsApp.hInst, NULL);
		// set in WM_CREATE
		_ASSERT(g_RecentItemsExclusionsApp.hWndSysTray);
		if (g_RecentItemsExclusionsApp.hWndSysTray)
		{
			SendMessage(g_RecentItemsExclusionsApp.hWndSysTray, RecentItemsExclusions::UWM_REGISTER_TRAY_ICON, 0, 0);
		}
	}
	return true;
}

void UnregisterTrayIcon()
{
	NOTIFYICONDATA ndata = {};
	memset(&ndata, 0, sizeof(NOTIFYICONDATA)); // redundant
	ndata.cbSize = sizeof(NOTIFYICONDATA);
	ndata.hWnd = g_RecentItemsExclusionsApp.hWndSysTray;
	ndata.uID = GetCurrentProcessId();	// our tray window ID will be our PID
	ndata.uCallbackMessage = RecentItemsExclusions::UWM_TRAY;
	Shell_NotifyIcon(NIM_DELETE, &ndata);
}

LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	static HICON s_hAppIcon = NULL;
	static UINT s_uTaskbarRestartMessageId = 0;
	static bool bNotificationWaiting_DoUpdate = false;
	switch (nMessage)
	{
	case WM_CREATE:
		// register to get taskbar recreation messages
		g_RecentItemsExclusionsApp.hWndSysTray = hWnd;
		s_uTaskbarRestartMessageId = RegisterWindowMessage(L"TaskbarCreated");
		s_hAppIcon = LoadIcon(g_RecentItemsExclusionsApp.hInst, MAKEINTRESOURCE(IDI_ICON1));
		PostMessage(hWnd, RecentItemsExclusions::UWM_START_UPDATE_CHECK_THREAD, 0, 0);
		PostMessage(hWnd, RecentItemsExclusions::UWM_START_PRUNING_THREAD, 0, 0);
		if (g_RecentItemsExclusionsApp.bOpenMainWindowAtStartup)
		{
			DEBUG_PRINT(L"-show switch given");
			PostMessage(hWnd, RecentItemsExclusions::UWM_OPEN_LIST_DIALOG, 0, 0);
		}
		return 0;
	case WM_DESTROY:
	{
		DEBUG_PRINT(L"WM_DESTROY");
		UnregisterTrayIcon();
		SetEvent(g_RecentItemsExclusionsApp.hExitEvent);
		PostQuitMessage(0);
		return 0;
	}
	case RecentItemsExclusions::UWM_EXIT:
	{
		DEBUG_PRINT(L"UWM_EXIT");
		DestroyWindow(hWnd);
		return 0;
	}
	case RecentItemsExclusions::UWM_START_PRUNING_THREAD:
	{
		g_PruningThread.Stop();		// ensure stopped
		std::vector<std::wstring> vMatchPhrases;
		g_RecentItemsExclusionsApp.ListSerializer.LoadListFromFile(g_RecentItemsExclusionsApp.strListSavePath, vMatchPhrases);
		g_PruningThread.Start(vMatchPhrases);
		return 0;
	}
	case RecentItemsExclusions::UWM_START_UPDATE_CHECK_THREAD:
	{
		DEBUG_PRINT(L"UWM_START_UPDATE_CHECK_THREAD");
		std::thread(UpdateCheckThread).detach();
		return 0;
	}
	case RecentItemsExclusions::UWM_STOP_PRUNING_THREAD:
		DEBUG_PRINT(L"UWM_STOP_PRUNING_THREAD");
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
	case RecentItemsExclusions::UWM_NEW_VERSION_AVAILABLE:
	{
		DEBUG_PRINT(L"Received UWM_NEW_VERSION_AVAILABLE, notifying user...");
		NOTIFYICONDATA ndata = {};
		memset(&ndata, 0, sizeof(ndata));
		ndata.cbSize = sizeof(NOTIFYICONDATA);
		ndata.hWnd = hWnd;
		ndata.uID = GetCurrentProcessId();	// our tray window ID will be our PID;
		ndata.uFlags = NIF_INFO;
		ndata.uTimeout = 10000;
		wcscpy_s(ndata.szInfoTitle, _countof(ndata.szInfoTitle) - 1, L"Update for " PRODUCT_NAME " is available");
		wsprintf(ndata.szInfo, L"Click here to update.");
		Shell_NotifyIcon(NIM_MODIFY, &ndata);
		bNotificationWaiting_DoUpdate = true;
	}
	break;
	case RecentItemsExclusions::UWM_OPEN_LIST_DIALOG:
	{
		if (!g_RecentItemsExclusionsApp.hWndListDialog)
		{
			if (DialogBoxParam(g_RecentItemsExclusionsApp.hInst, MAKEINTRESOURCE(IDD_EXCLUSIONS_LIST), NULL, &ListDialogProc, 0) == 0)
			{
				// reload list and restart thread
				// TODO: create watcher thread on config list in case externally changed
				std::vector<std::wstring> vStrings;
				g_RecentItemsExclusionsApp.ListSerializer.LoadListFromFile(g_RecentItemsExclusionsApp.strListSavePath, vStrings);
				SendMessage(hWnd, RecentItemsExclusions::UWM_START_PRUNING_THREAD, 0, 0);
			}
		}
		else
		{
			// bring existing to front
			SetForegroundWindow(g_RecentItemsExclusionsApp.hWndListDialog);
		}
	}
	break;

	case RecentItemsExclusions::UWM_TRAY:
		switch (lParam)
		{
		case NIN_BALLOONHIDE:
			DEBUG_PRINT(L"NIN_BALLOONHIDE");
			bNotificationWaiting_DoUpdate = false;
			break;
		case NIN_BALLOONTIMEOUT:
			DEBUG_PRINT(L"NIN_BALLOONTIMEOUT");
			bNotificationWaiting_DoUpdate = false;
			break;
		case NIN_BALLOONUSERCLICK:  // XP and above
			// clicked the balloon we last showed ... 	
			DEBUG_PRINT(L"NIM_BALLOONUSERCLICK");
			if (true == bNotificationWaiting_DoUpdate)
			{
				// calling this will cause the app to exit when downloaded installer is run
				if (!DownloadAndApplyUpdate())
				{
					MessageBox(hWnd, ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_UPDATE_ERROR_LAUNCHING).c_str(), PRODUCT_NAME, MB_ICONSTOP);
				}
				bNotificationWaiting_DoUpdate = false;
			}
			break;
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
			SendMessage(hWnd, RecentItemsExclusions::UWM_OPEN_LIST_DIALOG, 0, 0);
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

			unsigned long nTrackRes = (unsigned long)TrackPopupMenu(hMenuPopup,
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

void DeprecatedArtifactCleanup()
{
	RegDeleteKey(HKEY_CURRENT_USER, L"Software\\" PRODUCT_NAME);
	RegDeleteKey(HKEY_LOCAL_MACHINE, L"Software\\" PRODUCT_NAME);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pwCmdLine, int nShowCmd)
{
	g_RecentItemsExclusionsApp.hInst = g_RecentItemsExclusionsApp.hResourceModule = GetModuleHandle(nullptr);

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "show help")
		("close,c", "Close running instances.")
		("install,i", "Run install tasks.")
		("uninstall,u", "Run uninstall tasks.")
		("show,w", po::bool_switch(&g_RecentItemsExclusionsApp.bOpenMainWindowAtStartup), "Open main window at startup.");

	po::variables_map vm;
	po::store(po::command_line_parser(__argc, __argv).options(desc).allow_unregistered().run(), vm);
	po::notify(vm);

	if (vm.count("help"))
	{
		std::cerr << desc << "\n";
		return 1;
	}
	else if (vm.count("install"))
	{
		DEBUG_PRINT(L"-install");

		DeprecatedArtifactCleanup();

		CTaskScheduler TaskScheduler;
		WCHAR wszFile[MAX_PATH + 1] = { 0 };
		if (GetModuleFileName(NULL, wszFile, _countof(wszFile)) && wszFile[0])
		{
			// create startup task
			if (!TaskScheduler.CreateStartupTask(g_RecentItemsExclusionsApp.STARTUP_TASK_NAME, wszFile, L"-tray", NULL, true, false))
			{
				DEBUG_PRINT(L"ERROR: Could not create startup task. Make sure Task Scheduler service is started.");
				return 1;
			}
		}
		else
		{
			return 1;
		}
		return 0;
	}
	else if (vm.count("uninstall"))
	{
		DEBUG_PRINT(L"-uninstall");
		CTaskScheduler TaskScheduler;
		if (!TaskScheduler.RemoveStartupTask(g_RecentItemsExclusionsApp.STARTUP_TASK_NAME))
		{
			DEBUG_PRINT(L"ERROR removing Task Scheduler task");
			return 1;
		}
		RegDeleteKey(HKEY_CURRENT_USER, L"Software\\" PRODUCT_NAME_REG);
		RegDeleteKey(HKEY_LOCAL_MACHINE, L"Software\\" PRODUCT_NAME_REG);
		DeprecatedArtifactCleanup();
		return 0;
	}
	else if (vm.count("close"))
	{
		// first close the handle to our own exit event so we know when it disappears
		CloseHandle(g_RecentItemsExclusionsApp.hExternalExitSignal);
		g_RecentItemsExclusionsApp.hExternalExitSignal = NULL;

		HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, g_RecentItemsExclusionsApp.EXTERNAL_EXIT_SIGNAL_EVENT_NAME);
		if (hEvent)
		{
			SetEvent(hEvent);
			CloseHandle(hEvent);
			// wait until handle disappears to know when existing instances close, for a max of 10 seconds
			for (int i = 0; i < 10; i++)
			{
				hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, g_RecentItemsExclusionsApp.EXTERNAL_EXIT_SIGNAL_EVENT_NAME);
				if (hEvent)
				{
					CloseHandle(hEvent);
					Sleep(1000);
				}
				else
				{
					// event gone, now give it an extra 500ms to let a closing instance fully close
					// TODO: this is not the ideal wait mechanism
					Sleep(500);
					break;
				}
			}
			return 0;
		}
		return 1;
	}

	// if an existing instance exists, bring it to front and exit
	if (BringExistingInstanceToForeground())
	{
		DEBUG_PRINT(L"Existing instance opened");
		return 0;
	}

	// if beta version, turn include betas option on
#ifdef BETA_VERSION	
	if (!g_RecentItemsExclusionsApp.AreBetaUpdatesEnabled())
	{
		g_RecentItemsExclusionsApp.SetBetaUpdatesEnabled(true);
	}
#endif

	// initialize common controls
	INITCOMMONCONTROLSEX icex = {};
	icex.dwSize = sizeof(icex);
	icex.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_LINK_CLASS;
	if (!InitCommonControlsEx(&icex))
	{
		// W2K and XP pre-SP2
		InitCommonControls();
	}

	{
		ProductOptions prodOptions_Machine(HKEY_LOCAL_MACHINE, PRODUCT_NAME_REG);
		if (!prodOptions_Machine[L"darkmode_disable"])
		{
			// dark mode temporary disabled for further dev
			//InitDarkMode();
		}
	}

	CreateOrReinitializeTrayWindow(true);

	std::thread(ExitSignalWatchThread).detach();
	std::thread(PruningThreadStatusWatchThread).detach();

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
	// ensure tray icon unregistered
	// TODO: This should be redundant, see issue #10
	UnregisterTrayIcon();

	DEBUG_PRINT(L"app exiting");

	return 0;
}
