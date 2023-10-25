#pragma once
#include <algorithm>
#include <thread>
#include <vector>
#include <map>
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
			return L"STOPPED";
		case PruneThreadState_Monitoring:
			return L"MONITORING";
		case PruneThreadState_Pruning:
			return L"PRUNING";
		case PruneThreadState_Error:
			return L"ERROR";
		default:
			return L"UNKNOWN";
		}
	}

	void AddListeningEvent(const HANDLE hEvent)
	{
		RemoveListeningEvent(hEvent);	// make sure it's not already in the list
		m_vhListeningEventsForStatusChange.push_back(hEvent);
	}
	void RemoveListeningEvent(const HANDLE hEvent)
	{
		for (auto it = m_vhListeningEventsForStatusChange.begin(); it != m_vhListeningEventsForStatusChange.end(); ++it)
		{
			if (*it == hEvent)
			{
				m_vhListeningEventsForStatusChange.erase(it);
				break;
			}
		}
	}
	void NotifyAllListenersOfChange()
	{
		for (auto h : m_vhListeningEventsForStatusChange)
		{
			SetEvent(h);
		}
	}
	void Reset()
	{
		SetItemsLastScannedCount(0);
		SetTotalItemsPrunedCount(0);
		SetItemsPrunedTodayCount(0);
		NotifyAllListenersOfChange();
	}
	unsigned int GetMetric(const WCHAR* pwszName)
	{
		ProductOptions prodOptions(HKEY_CURRENT_USER, PRODUCT_NAME_REG);
		unsigned int nVal = 0;
		prodOptions.get_value(pwszName, nVal);
		return nVal;
	}
	unsigned int SetMetric(const WCHAR* pwszName, const unsigned int nVal)
	{
		ProductOptions prodOptions(HKEY_CURRENT_USER, PRODUCT_NAME_REG);
		prodOptions.set_value(pwszName, nVal);
		return nVal;
	}
	unsigned int GetTotalItemsPrunedCount()
	{
		return GetMetric(g_RecentItemsExclusionsApp.TOTAL_ITEMS_PRUNED_VALUENAME);
	}
	unsigned int SetTotalItemsPrunedCount(const unsigned int nCount)
	{
		return SetMetric(g_RecentItemsExclusionsApp.TOTAL_ITEMS_PRUNED_VALUENAME, nCount);
	}
	unsigned int GetItemsLastScannedCount()
	{
		return GetMetric(g_RecentItemsExclusionsApp.LAST_SCANNED_COUNT_VALUENAME);
	}
	unsigned int SetItemsLastScannedCount(const unsigned int nCount)
	{
		return SetMetric(g_RecentItemsExclusionsApp.LAST_SCANNED_COUNT_VALUENAME, nCount);
	}
	unsigned int GetItemsPrunedTodayCount()
	{
		ProductOptions prodOptions(HKEY_CURRENT_USER, PRODUCT_NAME_REG);
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
	unsigned int SetItemsPrunedTodayCount(const unsigned int nCount)
	{
		return SetMetric(g_RecentItemsExclusionsApp.ITEMS_PRUNED_TODAY_VALUENAME, nCount);
	}
};

//////////////////////////////////
// PruningThread

class PruningThread
{
private:
	HANDLE m_hExitEvent = NULL;
	std::thread m_threadPruner;

	enum FileScanResult { FileError, StringNotFound, StringFound };

	// below are for caching results so we don't have to rescan files when the match pattern set hasn't changed
	std::vector<std::wstring> m_vwstrLastPatternSet;
	std::map<std::wstring, FILETIME> m_mapFileToLastWriteAlreadyScannedWithPattern;
	void CacheUpdateLastSeenPatternSet(const std::vector<std::wstring>& vwstrPatternSet)
	{
		if (vwstrPatternSet != m_vwstrLastPatternSet)
		{
			DEBUG_PRINT(L"Updating pattern set and invalidating cache ...");
			m_vwstrLastPatternSet = vwstrPatternSet;
			m_mapFileToLastWriteAlreadyScannedWithPattern.clear();
		}
	}
	void CacheAddFile(const std::wstring& wstrPath, const FILETIME ftLastWrite)
	{
		m_mapFileToLastWriteAlreadyScannedWithPattern[wstrPath] = ftLastWrite;
		// if cache set exceeds max count, clear the cache and let it repopulate. Necessary due to potentially long-running nature of app, but still unlikely to ever be of issue.
		if (m_mapFileToLastWriteAlreadyScannedWithPattern.size() > RecentItemsExclusions::MAX_PRUNING_CACHE_ITEM_COUNT)
		{
			DEBUG_PRINT(L"Cache exceeds max size, clearing ...");
			m_mapFileToLastWriteAlreadyScannedWithPattern.clear();
		}
	}
	bool CacheIsInAlreadyScannedSet(const std::wstring& wstrPath, const FILETIME ftLastWrite)
	{
		auto i = m_mapFileToLastWriteAlreadyScannedWithPattern.find(wstrPath);
		if (i != m_mapFileToLastWriteAlreadyScannedWithPattern.end())
		{
			//if (CompareFileTime(&i->second, &ftLastWrite) == 0)
			// do manual member compare instead so we don't have to make an API call
			if (i->second.dwLowDateTime == ftLastWrite.dwLowDateTime
				&&
				i->second.dwHighDateTime == ftLastWrite.dwHighDateTime)
			{
				return true;
			}
		}
		return false;
	}

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
				// see rationale in DELAY_AFTER_FS_CHANGE_BEFORE_SCAN_MS defintion
				Sleep(RecentItemsExclusions::DELAY_AFTER_FS_CHANGE_BEFORE_SCAN_MS);

				DEBUG_PRINT(L"Change notification received, pruning ...");
				PruneFilesystem(vPaths, vSearchPatterns);

				if (!FindNextChangeNotification(hFindHandle))
				{
					DEBUG_PRINT(L"ERROR: Change notification failed.");
					bExit = true;
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
	FileScanResult DoesTextExistInFile(const std::wstring& filepath, const std::vector<std::wstring>& vSearchPatterns)
	{
		// read file into buffer
		HANDLE hFile = CreateFile(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			DEBUG_PRINT(L"ERROR: Opening %s", filepath.c_str());
			return FileError;
		}
		DWORD dwSizeHigh = 0;
		DWORD dwSizeLow = GetFileSize(hFile, &dwSizeHigh);
		FileScanResult scanResult = FileError;
		if (dwSizeLow && dwSizeLow != INVALID_FILE_SIZE)
		{
			DWORD dwBytesRead = 0;
			std::vector<char> buffer(dwSizeLow);
			if (ReadFile(hFile, buffer.data(), dwSizeLow, &dwBytesRead, NULL))
			{
				scanResult = StringNotFound;
				// search for text
				for (auto iPattern : vSearchPatterns)
				{
					if (std::search(buffer.begin(), buffer.end(), iPattern.begin(), iPattern.end(), [](wchar_t a, wchar_t b) { return std::toupper(a) == std::toupper(b); }) != buffer.end())
					{
						scanResult = StringFound;
					}
				}
			}
		}
		CloseHandle(hFile);
		return scanResult;
	}

	int PruneFilesystem(const std::vector<std::wstring>& vPaths, const std::vector<std::wstring>& vSearchPatterns)
	{
		DEBUG_PRINT(L"PruneFilesystem");
		CacheUpdateLastSeenPatternSet(vSearchPatterns);

		unsigned int nScannedCount = 0, nDeletedCount = 0, nSkippedCount = 0, nErrorCount = 0;

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
				if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) // if not directory
					&& findData.nFileSizeLow 	// and not empty
					&& !findData.nFileSizeHigh)	// and not too big
				{
					std::wstring filePath = path + L"\\" + findData.cFileName;

					if (CacheIsInAlreadyScannedSet(filePath, findData.ftLastWriteTime))
					{
						nSkippedCount++;
					}
					else
					{
						switch (DoesTextExistInFile(filePath, vSearchPatterns))
						{
						case StringFound:
							DEBUG_PRINT(L"Text found! Deleting file %s ...", findData.cFileName);
							if (!DeleteFile(filePath.c_str()))
							{
								DEBUG_PRINT(L"ERROR: Deleting file %s", filePath.c_str());
							}
							else
							{
								nDeletedCount++;
							}
							break;
						case StringNotFound:
							CacheAddFile(filePath, findData.ftLastWriteTime);
							break;
						case FileError:
							DEBUG_PRINT(L"ERROR: Accessing file %s", filePath.c_str());
							nErrorCount++;
							break;
						default:
							_ASSERT(0);
							break;
						}
					}
					nScannedCount++;
				}
			} while (FindNextFile(hFind, &findData));
			FindClose(hFind);
		}

		DEBUG_PRINT(L"Scanned %u items, skipped %u, deleted %u, errors %u", nScannedCount, nSkippedCount, nDeletedCount, nErrorCount);

		// update stats
		m_pruningStatus.m_pruneThreadState = PruningStatus::PruneThreadState_Monitoring;
		m_pruningStatus.SetItemsLastScannedCount(nScannedCount);
		m_pruningStatus.SetTotalItemsPrunedCount(m_pruningStatus.GetTotalItemsPrunedCount() + nDeletedCount);
		m_pruningStatus.SetItemsPrunedTodayCount(m_pruningStatus.GetItemsPrunedTodayCount() + nDeletedCount);
		m_pruningStatus.NotifyAllListenersOfChange();

		return 0;
	}
};