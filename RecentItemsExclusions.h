#pragma once

#include "../libcommon/libcommon/libCommon.h"
#include "ListSerializer.h"
#include "../libcommon/libcommon/ProductOptions.h"
#include "resource.h"
#include "versioninfo.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

class RecentItemsExclusions
{
public:
	RecentItemsExclusions()
	{
		// build up list config save path incrementally to ensure parent folders exist
		strListSavePath = GetAppDataPath();
		CreateDirectory(strListSavePath.c_str(), NULL);
		strListSavePath += L"\\RecentItemsExclusions";
		CreateDirectory(strListSavePath.c_str(), NULL);
		strListSavePath += L"\\config";
		CreateDirectory(strListSavePath.c_str(), NULL);
		strListSavePath += L"\\RecentItemsExclusions.txt";

		hExitEvent = CreateEvent(nullptr,
			TRUE, // manual reset for exit events, since we set once and have multiple listeners
			FALSE, nullptr);
		_ASSERT(hExitEvent);

		hExternalExitSignal = CreateEvent(nullptr,
			TRUE, // manual reset for exit events, since we set once and have multiple listeners
			FALSE, EXTERNAL_EXIT_SIGNAL_EVENT_NAME);
		_ASSERT(hExternalExitSignal);

		hPruningThreadStatusChangedEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		_ASSERT(hPruningThreadStatusChangedEvent);		
	}
	~RecentItemsExclusions()
	{
		if (hPruningThreadStatusChangedEvent)
		{
			CloseHandle(hPruningThreadStatusChangedEvent);
		}		
		if (hExternalExitSignal)
		{
			CloseHandle(hExternalExitSignal);
		}
		if (hExitEvent)
		{
			CloseHandle(hExitEvent);
		}
	}	

	const WCHAR* INSTALLER_FILENAME = L"RecentItemsExclusionsSetup.exe";
	const static unsigned long MINIMUM_VALID_INSTALLER_SIZE = 16 * 1024;
	const WCHAR* UPDATE_CHECKS_DISABLED_VALUENAME = L"UpdateChecksDisabled";	// inverse so we default to enabled
	const WCHAR* BETA_UPDATES_VALUENAME = L"BetaUpdates";
	const WCHAR* UPDATE_CHECK_URL = L"https://update.bitsum.com/versioninfo/recentitemsexclusions/";
	const static unsigned long UPDATE_CHECK_INTERVAL_MS = 1000 * 60 * 60 * 24;	// 1 day
	std::wstring strFetechedVersionAvailableForDownload;

	ListSerializer ListSerializer;
	std::wstring strListSavePath;
	const WCHAR* EXTERNAL_EXIT_SIGNAL_EVENT_NAME = L"Global\\{4ceb4f3d-9838-4153-a6a5-3f004a563133}";
	const WCHAR* SYSTRAY_WINDOW_CLASS_NAME = L"RecentItemsExclusions_TrayClass";
	const WCHAR* SYSTRAY_WINDOW_NAME = L"RecentItemsExclusions_TrayWnd";
	const static UINT UWM_TRAY = WM_USER + 1;
	const static UINT UWM_REGISTER_TRAY_ICON = WM_USER + 2;
	const static UINT UWM_START_PRUNING_THREAD = WM_USER + 3;
	const static UINT UWM_STOP_PRUNING_THREAD = WM_USER + 4;
	const static UINT UWM_NEW_VERSION_AVAILABLE = WM_USER + 5;
	const static UINT UWM_START_UPDATE_CHECK_THREAD = WM_USER + 6;
	const static UINT UWM_EXIT = WM_USER + 7;
	const static UINT UWM_OPEN_LIST_DIALOG = WM_USER + 8;
	const static UINT UWM_STATUS_CHANGED = WM_USER + 9;

	HINSTANCE hInst = NULL;
	HMODULE hResourceModule = NULL;
	HWND hWndSysTray = NULL;
	HWND hWndListDialog = NULL;
	HANDLE hExitEvent;
	HANDLE hExternalExitSignal;
	HANDLE hPruningThreadStatusChangedEvent;
	
	const WCHAR* TOTAL_ITEMS_PRUNED_VALUENAME = L"TotalItemsPruned";
	const WCHAR* ITEMS_PRUNED_TODAY_VALUENAME = L"ItemsPrunedToday";
	const WCHAR* LAST_DAY_VALUENAME = L"LastDay";

	void SetUpdateChecksEnabled(const bool bVal)
	{
		ProductOptions prodOptions_User(HKEY_CURRENT_USER, PRODUCT_NAME);
		ProductOptions prodOptions_Machine(HKEY_LOCAL_MACHINE, PRODUCT_NAME);
		prodOptions_User.set_value(UPDATE_CHECKS_DISABLED_VALUENAME, !bVal);
		prodOptions_Machine.set_value(UPDATE_CHECKS_DISABLED_VALUENAME, !bVal);
		return;
	}

	bool AreUpdateChecksEnabled()
	{
		ProductOptions prodOptions_User(HKEY_CURRENT_USER, PRODUCT_NAME);
		ProductOptions prodOptions_Machine(HKEY_LOCAL_MACHINE, PRODUCT_NAME);
		if (prodOptions_User[UPDATE_CHECKS_DISABLED_VALUENAME]
			||
			prodOptions_Machine[UPDATE_CHECKS_DISABLED_VALUENAME])
		{
			return false;
		}
		return true;
	}

	void SetBetaUpdatesEnabled(const bool bVal)
	{
		ProductOptions prodOptions_User(HKEY_CURRENT_USER, PRODUCT_NAME);
		ProductOptions prodOptions_Machine(HKEY_LOCAL_MACHINE, PRODUCT_NAME);
		prodOptions_User.set_value(BETA_UPDATES_VALUENAME, bVal);
		prodOptions_Machine.set_value(BETA_UPDATES_VALUENAME, bVal);
		return;
	}

	bool AreBetaUpdatesEnabled()
	{
		ProductOptions prodOptions_User(HKEY_CURRENT_USER, PRODUCT_NAME);
		ProductOptions prodOptions_Machine(HKEY_LOCAL_MACHINE, PRODUCT_NAME);
		if (prodOptions_User[BETA_UPDATES_VALUENAME]
			||
			prodOptions_Machine[BETA_UPDATES_VALUENAME])
		{
			return true;
		}
		return false;
	}
};

bool BringExistingInstanceToForeground();
bool CreateOrReinitializeTrayWindow(const bool bFirstTimeCreation = true);
LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam);