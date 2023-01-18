#pragma once
#include <algorithm>
#include <thread>
#include <vector>
#include <string>
#include <windows.h>
#include <Shlobj.h>
#include "DebugOut.h"
#include "../libcommon/libcommon/ResourceHelpers.h"
#include "../libcommon/libcommon/ProductOptions.h"
#include "RecentItemsExclusions.h"
#include "versioninfo.h"

extern RecentItemsExclusions g_RecentItemsExclusionsApp;	// app globals and config

class PruningStatus
{
	std::vector<HANDLE> m_vhListeningEventsForStatusChange;	// clients register these to know when the status changes
public:
	enum PruneThreadState { PruneThreadState_Stopped, PruneThreadState_Monitoring, PruneThreadState_Pruning, PruneThreadState_Error };
	PruneThreadState m_pruneThreadState = PruneThreadState_Stopped;
	std::wstring GetStateString()
	{
		switch (m_pruneThreadState)
		{
		case PruneThreadState_Stopped:
			return L"Stopped";
		case PruneThreadState_Monitoring:
			return L"Monitoring";
		case PruneThreadState_Pruning:
			return L"Pruning";
		case PruneThreadState_Error:
			return L"Error";
		default:
			return L"Unknown";
		}
	}

	unsigned long m_nTotalItemsLastScanned;

	void AddListeningEvent(HANDLE hEvent)
	{
		m_vhListeningEventsForStatusChange.push_back(hEvent);
	}
	void NotifyAllListenersOfChange()
	{
		for (auto h : m_vhListeningEventsForStatusChange)
		{
			SetEvent(h);
		}
	}
	unsigned int GetTotalItemsPrunedCount()
	{
		ProductOptions prodOptions(HKEY_CURRENT_USER, PRODUCT_NAME);
		unsigned int nCount = 0;
		prodOptions.get_value(g_RecentItemsExclusionsApp.TOTAL_ITEMS_PRUNED_VALUENAME, nCount);
		return nCount;
	}
	unsigned int SetTotalItemsPrunedCount(unsigned int nCount)
	{
		ProductOptions prodOptions(HKEY_CURRENT_USER, PRODUCT_NAME);
		prodOptions.set_value(g_RecentItemsExclusionsApp.TOTAL_ITEMS_PRUNED_VALUENAME, nCount);
		return nCount;
	}
	int GetItemsPrunedTodayCount()
	{
		ProductOptions prodOptions(HKEY_CURRENT_USER, PRODUCT_NAME);
		// check if day is different than last recorded. If so, reset counter.
		SYSTEMTIME sysTime = {};
		GetSystemTime(&sysTime);
		unsigned long nTodayEncoded = sysTime.wYear * 10000 + sysTime.wMonth * 100 + sysTime.wDay;
		unsigned long nLastDayEncoded = 0;
		prodOptions.get_value(g_RecentItemsExclusionsApp.LAST_DAY_VALUENAME, nLastDayEncoded);
		if (nLastDayEncoded != nTodayEncoded)
		{
			DEBUG_PRINT(L"Day changed, clearing today's count.");
			prodOptions.set_value(g_RecentItemsExclusionsApp.LAST_DAY_VALUENAME, nTodayEncoded);
			SetItemsPrunedTodayCount(0);
		}
		unsigned int nCount = 0;
		prodOptions.get_value(g_RecentItemsExclusionsApp.ITEMS_PRUNED_TODAY_VALUENAME, nCount);
		return nCount;
	}
	unsigned long SetItemsPrunedTodayCount(unsigned long nCount)
	{
		ProductOptions prodOptions(HKEY_CURRENT_USER, PRODUCT_NAME);
		prodOptions.set_value(g_RecentItemsExclusionsApp.ITEMS_PRUNED_TODAY_VALUENAME, nCount);
		return nCount;
	}
};

class PruningThread
{
private:
	HANDLE m_hExitEvent = NULL;
	std::thread m_threadPruner;

public:
	PruningThread() = default;
	~PruningThread()
	{
		Stop();
	}
	PruningStatus m_pruningStatus;
	bool IsStarted()
	{
		return (m_hExitEvent != NULL);
	}
	bool Start(const std::vector<std::wstring> vSearchPatterns)
	{
		_ASSERT(!m_hExitEvent);
		m_hExitEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		m_threadPruner = std::thread(&PruningThread::FilesystemPruningThread, this, vSearchPatterns);
		return true;
	}
	bool Stop()
	{
		if (IsStarted())
		{
			SetEvent(m_hExitEvent);
			CloseHandle(m_hExitEvent);
			m_threadPruner.join();
			m_hExitEvent = NULL;
			return true;
		}
		return false;
	}
private:
	int FilesystemPruningThread(const std::vector<std::wstring> vSearchPatterns)
	{
		DEBUG_PRINT(L"FilesystemPruningThread begin");
		std::wstring pathRecentItems;
		{
			WCHAR* pwszPath = nullptr;			
			if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Recent, 0, NULL, &pwszPath)))
			{
				pathRecentItems = pwszPath;
			}
			// MSDN says to free pwszPath regardless of result of SHGetKnownFolderPath, so do so if populated, even though it makes this code ugly
			if (pwszPath)
			{
				CoTaskMemFree(pwszPath);
			}
		}
		if (pathRecentItems.empty())
		{
			DEBUG_PRINT(L"ERROR resolving Recent Items path");
			return -1;
		}
		DEBUG_PRINT(L"Path: %s", pathRecentItems.c_str());

		// build the paths we want to check
		std::vector<std::wstring> vPaths;
		vPaths.push_back(pathRecentItems);
		vPaths.push_back(pathRecentItems + L"\\AutomaticDestinations");
		vPaths.push_back(pathRecentItems + L"\\CustomDestinations");

		// do an initial prune
		PruneFilesystem(vPaths, vSearchPatterns);

		HANDLE hFindHandle = FindFirstChangeNotification(pathRecentItems.c_str(), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME);
		if (!hFindHandle)
		{
			DEBUG_PRINT(L"ERROR: Can't register change notification.");
			return -2;
		}

		HANDLE hWaitHandles[2] = { m_hExitEvent, hFindHandle };
		bool bExit = false;
		while (!bExit)
		{
			DWORD dwWaitStatus = WaitForMultipleObjects(_countof(hWaitHandles), hWaitHandles, FALSE, INFINITE);
			switch (dwWaitStatus)
			{
			case WAIT_OBJECT_0:
				DEBUG_PRINT(L"PruningThread Exit event received");
				bExit = true;
				break;
			case WAIT_OBJECT_0 + 1:
				DEBUG_PRINT(L"Change notification received, pruning ...");
				PruneFilesystem(vPaths, vSearchPatterns);
				if (!FindNextChangeNotification(hFindHandle))
				{
					DEBUG_PRINT(L"ERROR: Change notification failed.");
					bExit = true;
				}
				else
				{
					// Safety sleep in case monitored folder has rapid, continuous changes, causing this thread to hog CPU
					//  Trade-off is this could delay processing of rapidly fired changes.
					Sleep(100);
				}
				break;
			default:
				DEBUG_PRINT(L"ERROR: Unexpected wait result");
				bExit = true;
				break;
			}
		}

		FindCloseChangeNotification(hFindHandle);
		DEBUG_PRINT(L"FilesystemPruningThread end");
		return 0;
	}
	bool DoesTextExistInFile(const std::wstring& filepath, const std::vector<std::wstring>& vSearchPatterns)
	{
		// read file into buffer
		HANDLE hFile = CreateFile(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			DEBUG_PRINT(L"ERROR: Opening %s", filepath.c_str());
			return false;
		}
		DWORD dwSizeHigh = 0;
		DWORD dwSizeLow = GetFileSize(hFile, &dwSizeHigh);
		bool bTextFound = false;
		if (dwSizeLow != INVALID_FILE_SIZE)
		{
			DWORD dwBytesRead = 0;
			std::vector<char> buffer(dwSizeLow);
			if (ReadFile(hFile, buffer.data(), dwSizeLow, &dwBytesRead, NULL))
			{
				// search for text
				for (auto iPattern : vSearchPatterns)
				{
					if (std::search(buffer.begin(), buffer.end(), iPattern.begin(), iPattern.end(), [](wchar_t a, wchar_t b) { return std::toupper(a) == std::toupper(b); }) != buffer.end())
					{
						bTextFound = true;
						break;
					}
				}
			}
		}
		CloseHandle(hFile);
		return bTextFound;
	}

	int PruneFilesystem(const std::vector<std::wstring>& vPaths, const std::vector<std::wstring>& vSearchPatterns)
	{
		DEBUG_PRINT(L"PruneFilesystem");

		unsigned int nScannedCount = 0, nDeletedCount = 0;

		m_pruningStatus.m_pruneThreadState = PruningStatus::PruneThreadState_Pruning;
		m_pruningStatus.NotifyAllListenersOfChange();
		
		for (auto path : vPaths)
		{
			WIN32_FIND_DATA findData = {};
			std::wstring filespec = path + L"\\*";
			HANDLE hFind = FindFirstFile(filespec.c_str(), &findData);
			if (hFind == INVALID_HANDLE_VALUE)
			{
				DEBUG_PRINT(L"ERROR: Enumerating files in %s", path.c_str());
				return -1;
			}
			do
			{
				if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					std::wstring filePath = path + L"\\" + findData.cFileName;
					if (DoesTextExistInFile(filePath, vSearchPatterns))
					{
						DEBUG_PRINT(L"Text found! Deleting file %s ...", findData.cFileName);
						if (!DeleteFile(filePath.c_str()))
						{
							DEBUG_PRINT(L"ERROR: Deleting file %s", filePath.c_str());
						}
						else
						{
							nDeletedCount++;
						}
					}
				}
				nScannedCount++;
			} while (FindNextFile(hFind, &findData));
			FindClose(hFind);
		}

		// update stats
		m_pruningStatus.m_pruneThreadState = PruningStatus::PruneThreadState_Monitoring;		
		m_pruningStatus.m_nTotalItemsLastScanned = nScannedCount;
		m_pruningStatus.SetTotalItemsPrunedCount(m_pruningStatus.GetTotalItemsPrunedCount() + nDeletedCount);
		m_pruningStatus.SetItemsPrunedTodayCount(m_pruningStatus.GetItemsPrunedTodayCount() + nDeletedCount);
		m_pruningStatus.NotifyAllListenersOfChange();
		return 0;
	}
};