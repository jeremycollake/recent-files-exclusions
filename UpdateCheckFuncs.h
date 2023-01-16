/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#pragma once

#include <windows.h>

DWORD FetchLatestVersionNumberAsync(const HANDLE hEventComplete, _Out_ std::wstring* pwstrResult);
DWORD WINAPI FetchLatestVersionNumber(const HANDLE hEventComplete, _Out_ std::wstring* pwstrResult);

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
