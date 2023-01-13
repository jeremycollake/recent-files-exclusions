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

bool DoesTextExistInFile(const std::wstring& filepath, const std::vector<std::wstring>& vSearchPatterns)
{
	// read file into buffer
	HANDLE hFile = CreateFile(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		wprintf(L"\n ERROR: Opening %s", filepath.c_str());
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
	for (auto path : vPaths)
	{
		WIN32_FIND_DATA findData = {};
		std::wstring filespec = path + L"\\*";
		HANDLE hFind = FindFirstFile(filespec.c_str(), &findData);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			wprintf(L"\n ERROR: Enumerating files in %s", path.c_str());
			return -1;
		}
		do
		{
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				wprintf(L"\n\t - %s", findData.cFileName);
				std::wstring filePath = path + L"\\" + findData.cFileName;
				if (DoesTextExistInFile(filePath, vSearchPatterns))
				{
					wprintf(L"\n\t\t Text found! Deleting file ...");
					if (!DeleteFile(filePath.c_str()))
					{
						wprintf(L"\n ERROR: Deleting file %s", filePath.c_str());
					}
				}
			}
		} while (FindNextFile(hFind, &findData));
		FindClose(hFind);
	}
	return 0;
}

int FilesystemPruningThread(const HANDLE hExitEvent, const std::vector<std::wstring>& vSearchPatterns)
{
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
		wprintf(L"\n ERROR resolving Recent Items path");
		return -1;
	}
	wprintf(L"\n Path: %s", pathRecentItems.c_str());

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
		wprintf(L"\n ERROR: Can't register change notification.");
		return -2;
	}

	HANDLE hWaitHandles[2] = { hExitEvent, hFindHandle };
	bool bExit = false;
	while (!bExit)
	{
		DWORD dwWaitStatus = WaitForMultipleObjects(_countof(hWaitHandles), hWaitHandles, FALSE, INFINITE);
		switch (dwWaitStatus)
		{
		case WAIT_OBJECT_0:
			wprintf(L"\n Exit event received");
			bExit = true;
			break;
		case WAIT_OBJECT_0 + 1:
			wprintf(L"\n Change notification received, pruning ...");
			PruneFilesystem(vPaths, vSearchPatterns);
			if (!FindNextChangeNotification(hFindHandle))
			{
				wprintf(L"\n ERROR: Change notification failed.");
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
			wprintf(L"\n ERROR: Unexpected wait result");
			bExit = true;
			break;
		}
	}

	FindCloseChangeNotification(hFindHandle);

	return 0;
}

int wmain(int argc, wchar_t* argv[])
{
	wprintf(L"\nQuick Access Exclusions\n");
	std::vector<std::wstring> vSearchPatterns;
	for (int i = 1; i < argc; i++)
	{
		vSearchPatterns.push_back(argv[i]);
	}
	HANDLE hExitEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	std::thread workThread(FilesystemPruningThread, hExitEvent, vSearchPatterns);
	wprintf(L"\n Press any key to exit ...");
	while (!_kbhit());
	SetEvent(hExitEvent);
	CloseHandle(hExitEvent);
	workThread.join();
	return 0;
}
