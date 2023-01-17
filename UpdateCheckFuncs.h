#pragma once

#include <windows.h>

#pragma comment (lib, "wininet")

DWORD FetchLatestVersionNumberAsync(_Out_ std::wstring* pwstrResult, const HANDLE hEventComplete);
// use FetchLatestVersionNumberAsync to spawn and detach this thread
DWORD FetchLatestVersionNumber(_Out_ std::wstring* pwstrResult, const HANDLE hEventComplete);

bool DownloadAndApplyUpdate();

bool DownloadText(const WCHAR* pwszUserAgent,
	const WCHAR* pwszUrl,
	char** ppszNewBuffer,
	int* pnReturnedBufferSizeInBytes);

bool DownloadBinaryFile(const WCHAR* pwszUserAgent,
	const WCHAR* pwszUrl,
	char** ppBuffer,
	int* pnReturnedBufferSizeInBytes);

unsigned long TextVersionToULONG(const WCHAR* pwszVer);
