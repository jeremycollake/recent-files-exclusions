#pragma once

#include "../libcommon/libcommon/libCommon.h"
#include "ListSerializer.h"
#include "../libcommon/libcommon/ProductOptions.h"
#include "TaskScheduler.h"
#include "resource.h"
#include "versioninfo.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

class RecentItemsExclusions
{
	ProductOptions* pProdOptions_Machine = new ProductOptions(HKEY_LOCAL_MACHINE, PRODUCT_NAME_REG);
	ProductOptions* pProdOptions_User = new ProductOptions(HKEY_CURRENT_USER, PRODUCT_NAME_REG);

public:
	RecentItemsExclusions()
	{
		// build up list config save path incrementally to ensure parent folders exist
		strListSavePath = GetAppDataPath();
		CreateDirectory(strListSavePath.c_str(), NULL);
		strListSavePath += L"\\RecentFilesExclusions";
		CreateDirectory(strListSavePath.c_str(), NULL);
		strListSavePath += L"\\config";
		CreateDirectory(strListSavePath.c_str(), NULL);
		strListSavePath += L"\\exclusions.txt";

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
		if (pProdOptions_Machine)
		{
			delete pProdOptions_Machine;
		}
		if (pProdOptions_User)
		{
			delete pProdOptions_User;
		}
	}

	// if cache set exceeds this count, clear the cache and let it repopulate. Necessary due to potentially long-running nature of app, but still unlikely to ever be of issue.
	const static unsigned long MAX_PRUNING_CACHE_ITEM_COUNT = 16 * 1024;

	// this delay has two purposes:
	//  1. Give filesystem changes a chance to settle
	//  2. A rate-limiting safety in the worst edge case of rapid, frequent changes
	const static unsigned long DELAY_AFTER_FS_CHANGE_BEFORE_SCAN_MS = 100;

	const WCHAR* SETUP_FILE_URL_BASE_PATH = L"https://dl.bitsum.com/files/";
	const WCHAR* INSTALLER_FILENAME = L"RecentFilesExclusionsSetup.exe";
	const WCHAR* STARTUP_TASK_NAME = L"Recent Files Exclusions";
	const static unsigned long MINIMUM_VALID_INSTALLER_SIZE = 16 * 1024;
	const WCHAR* UPDATE_CHECKS_DISABLED_VALUENAME = L"UpdateChecksDisabled";	// inverse so we default to enabled
	const WCHAR* BETA_UPDATES_VALUENAME = L"BetaUpdates";
	const WCHAR* UPDATE_CHECK_URL = L"https://update.bitsum.com/versioninfo/recentfilesexclusions/";
	const static unsigned long UPDATE_CHECK_INTERVAL_MS = 1000 * 60 * 60 * 24;	// 1 day
	std::wstring strFetechedVersionAvailableForDownload;

	const WCHAR* OPTION_NAME_PRUNE_RECENTITEMS = L"PruneRecentItems";
	const bool OPTION_DEFAULT_PRUNE_RECENTITEMS = true;
	const WCHAR* OPTION_NAME_PRUNE_AUTODEST = L"PruneAutomaticDestinations";
	const bool OPTION_DEFAULT_PRUNE_AUTODEST = true;
	const WCHAR* OPTION_NAME_PRUNE_CUSTOMDEST = L"PruneCustomDestinations";
	const bool OPTION_DEFAULT_PRUNE_CUSTOMDEST = false;

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

	bool bOpenMainWindowAtStartup = false;
	HINSTANCE hInst = NULL;
	HMODULE hResourceModule = NULL;
	HWND hWndSysTray = NULL;
	HWND hWndListDialog = NULL;
	HANDLE hExitEvent;
	HANDLE hExternalExitSignal;
	HANDLE hPruningThreadStatusChangedEvent;

	const COLORREF DARK_WINDOW_BACKGROUND = 0x383838;
	const COLORREF DARK_WINDOW_TEXT_COLOR = 0xFFFFFF;

	const WCHAR* LAST_SCANNED_COUNT_VALUENAME = L"ItemsLastScannedCount";
	const WCHAR* TOTAL_ITEMS_PRUNED_VALUENAME = L"TotalItemsPrunedCount";
	const WCHAR* ITEMS_PRUNED_TODAY_VALUENAME = L"ItemsPrunedTodayCount";
	const WCHAR* LAST_DAY_VALUENAME = L"LastDay";

	bool GetNamedBooleanOptionValue(const WCHAR* pwszName, const bool bForAllUsers, const bool bDefault)
	{
		bool bVal = bDefault;
		if (bForAllUsers)
		{
			pProdOptions_Machine->get_value(pwszName, bVal, bDefault);
		}
		else
		{
			pProdOptions_User->get_value(pwszName, bVal, bDefault);
		}
		return bVal;
	}

	bool SetNamedBooleanOptionValue(const WCHAR* pwszName, const bool bForAllUsers, const bool bVal)
	{
		if (bForAllUsers)
		{
			return pProdOptions_Machine->set_value(pwszName, bVal);
		}
		return pProdOptions_User->set_value(pwszName, bVal);
	}

	void SetUpdateChecksEnabled(const bool bVal)
	{
		// inverted bool, should rename this method to 'Disabled'
		SetNamedBooleanOptionValue(UPDATE_CHECKS_DISABLED_VALUENAME, true, !bVal);
		SetNamedBooleanOptionValue(UPDATE_CHECKS_DISABLED_VALUENAME, false, !bVal);
		return;
	}

	bool AreUpdateChecksEnabled()
	{
		// inverted bool, should rename this method to 'Disabled'
		return !(GetNamedBooleanOptionValue(UPDATE_CHECKS_DISABLED_VALUENAME, true, false)
			||
			GetNamedBooleanOptionValue(UPDATE_CHECKS_DISABLED_VALUENAME, false, false));
	}

	void SetBetaUpdatesEnabled(const bool bVal)
	{
		SetNamedBooleanOptionValue(BETA_UPDATES_VALUENAME, true, bVal);
		SetNamedBooleanOptionValue(BETA_UPDATES_VALUENAME, false, bVal);
	}

	bool AreBetaUpdatesEnabled()
	{
		return (GetNamedBooleanOptionValue(BETA_UPDATES_VALUENAME, true, false)
			||
			GetNamedBooleanOptionValue(BETA_UPDATES_VALUENAME, false, false));
	}

	bool GetShouldPruneRecentItems()
	{
		return GetNamedBooleanOptionValue(OPTION_NAME_PRUNE_RECENTITEMS, false, OPTION_DEFAULT_PRUNE_RECENTITEMS);
	}
	bool SetShouldPruneRecentItems(const bool bVal)
	{
		return SetNamedBooleanOptionValue(OPTION_NAME_PRUNE_RECENTITEMS, false, bVal);
	}
	bool GetShouldPruneAutoDest()
	{
		return GetNamedBooleanOptionValue(OPTION_NAME_PRUNE_AUTODEST, false, OPTION_DEFAULT_PRUNE_AUTODEST);
	}
	bool SetShouldPruneAutoDest(const bool bVal)
	{
		return SetNamedBooleanOptionValue(OPTION_NAME_PRUNE_AUTODEST, false, bVal);
	}
	bool GetShouldPruneCustomDest()
	{
		return GetNamedBooleanOptionValue(OPTION_NAME_PRUNE_CUSTOMDEST, false, OPTION_DEFAULT_PRUNE_CUSTOMDEST);
	}
	bool SetShouldPruneCustomDest(const bool bVal)
	{
		return SetNamedBooleanOptionValue(OPTION_NAME_PRUNE_CUSTOMDEST, false, bVal);
	}
};

void DeprecatedArtifactCleanup();
bool BringExistingInstanceToForeground();
bool CreateOrReinitializeTrayWindow(const bool bFirstTimeCreation = true);
LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam);