#include <Windows.h>
#include <WinInet.h>
#include <shellapi.h>
#include <thread>
#include "../libcommon/libcommon/ResourceHelpers.h"
#include "../libcommon/libcommon/ProductOptions.h"
#include "UpdateCheckFuncs.h"
#include "RecentItemsExclusions.h"
#include "SignatureVerifier.h"

extern RecentItemsExclusions g_RecentItemsExclusionsApp;	// app globals and config

// TODO: encapsulate these functions

// versioninfo signature indicating a valid response from the server (TODO: refactor versioninfo result to JSON or other structured data)
#define BITSUM_UPDATE_MAGIC_SIG L"bitsum_updatevalidsig"

DWORD FetchLatestVersionNumberAsync(_Out_ std::wstring* pwstrResult, const HANDLE hEventComplete)
{
	std::thread checkThread(FetchLatestVersionNumber, pwstrResult, hEventComplete);
	checkThread.detach();
	return GetThreadId(checkThread.native_handle());
}

// use FetchLatestVersionNumberAsync to spawn and detach this thread
DWORD FetchLatestVersionNumber(_Out_ std::wstring* pwstrResult, const HANDLE hEventComplete)
{
	pwstrResult->clear();

	std::wstring strUrl = g_RecentItemsExclusionsApp.UPDATE_CHECK_URL;
	if (g_RecentItemsExclusionsApp.AreBetaUpdatesEnabled())
	{
		strUrl += L"beta/";
	}

	char* pszVersionBuffer = NULL;
	int nBytesRead = 0;

	if (false == DownloadText(PRODUCT_USER_AGENT, strUrl.c_str(), &pszVersionBuffer, &nBytesRead)
		|| NULL == pszVersionBuffer || 0 == strlen(pszVersionBuffer))
	{
		DEBUG_PRINT(L"ERROR checking update");

		if (pszVersionBuffer != nullptr)
		{
			delete pszVersionBuffer;
		}
		if (hEventComplete)
		{
			SetEvent(hEventComplete);
		}
		return 1;
	}

	std::wstring strVersion = convert_to_wstring(pszVersionBuffer);

	delete pszVersionBuffer;
	pszVersionBuffer = nullptr;

	// basic check for bad result	
	size_t nFirstNewline = strVersion.find_first_of(L"\r\n");
	if (strVersion.length() < 3
		||
		nFirstNewline == std::wstring::npos
		||
		strVersion.find(L".") == std::wstring::npos
		||
		strVersion.find(BITSUM_UPDATE_MAGIC_SIG) == std::wstring::npos)
	{
		DEBUG_PRINT(L"Updater bad result");
		if (hEventComplete)
		{
			SetEvent(hEventComplete);
		}
		return 0;
	}

	*pwstrResult = strVersion.substr(0, nFirstNewline);

	if (hEventComplete)
	{
		SetEvent(hEventComplete);
	}
	return 0;
}

bool DownloadAndApplyUpdate()
{
	CString csTargetSavePath, csTempPath;
	{
		WCHAR wszTempPath[MAX_PATH] = { 0 };
		GetTempPath(_countof(wszTempPath), wszTempPath);
		csTempPath = wszTempPath;
		AppendBackslashIfMissing(csTempPath);
	}
	csTargetSavePath.Format(L"%s%s", csTempPath, L"bitsum");
	CreateDirectory(csTargetSavePath, NULL);
	csTargetSavePath.AppendFormat(L"\\%s", g_RecentItemsExclusionsApp.INSTALLER_FILENAME);

	DEBUG_PRINT(L"Download save path: %s", csTargetSavePath);

	char* pBuffer = NULL;		// dynamically allocated and retunred by DownloadFile if success
	int nDownloadSizeInBytes = 0;

	std::wstring strDownloadUrl = g_RecentItemsExclusionsApp.SETUP_FILE_URL_BASE_PATH;
	if (g_RecentItemsExclusionsApp.AreBetaUpdatesEnabled())
	{
		strDownloadUrl += L"beta/";
	}
	strDownloadUrl += g_RecentItemsExclusionsApp.INSTALLER_FILENAME;
	DEBUG_PRINT(L"Downloading %s", strDownloadUrl.c_str());
	if (false == DownloadBinaryFile(PRODUCT_USER_AGENT, strDownloadUrl.c_str(), &pBuffer, &nDownloadSizeInBytes) || NULL == pBuffer || nDownloadSizeInBytes <= g_RecentItemsExclusionsApp.MINIMUM_VALID_INSTALLER_SIZE)
	{
		DEBUG_PRINT(L"ERROR downloading file");
		return false;
	}
	HANDLE hFile = CreateFile(csTargetSavePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		DEBUG_PRINT(L"ERROR creating file");
		return false;
	}
	DWORD dwBytesWritten = 0;
	if (FALSE == WriteFile(hFile, pBuffer, nDownloadSizeInBytes, &dwBytesWritten, NULL) || dwBytesWritten != nDownloadSizeInBytes)
	{
		DEBUG_PRINT(L"ERROR writing file");
		CloseHandle(hFile);
		return false;
	}
	CloseHandle(hFile);

	// verify digital signature
	SignatureVerifier SignVerifier;
	LONG lStatus = SignVerifier.VerifyEmbeddedSignature(csTargetSavePath);
	if (lStatus != ERROR_SUCCESS)
	{
		DEBUG_PRINT(L"ERROR validating signature of update package");
		ATL::CString csTemp;
		std::wstring strErrorMsg = SignVerifier.TranslateWinVerifySigningStatusToMessage(lStatus);
		csTemp.Format(ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_UPDATE_SIGNED_FAILURE_MESSAGE_FMT).c_str(), strErrorMsg.c_str());
		DEBUG_PRINT(csTemp);
		if (MessageBox(NULL, csTemp, PRODUCT_NAME, MB_ICONWARNING | MB_APPLMODAL | MB_YESNO) == IDYES)
		{
			ShellExecute(NULL, NULL, PRODUCT_URL L"/?prod=rie&update_error_sig", NULL, NULL, SW_SHOWNORMAL);
		}
		return false;
	}

	DEBUG_PRINT(L"Launching installer %s ...", csTargetSavePath);

	HANDLE hProcess = LaunchProcessWithElevation(csTargetSavePath, L"/S", nullptr);

	if (!hProcess)
	{
		DEBUG_PRINT(L"ERROR Launching %s", csTargetSavePath);
	}
	else
	{
		CloseHandle(hProcess);
	}

	// caller is now expected to exit, though installer should terminate the process anyway -- so is optional
	return hProcess ? true : false;
}


size_t AddNullTerminatorToBuffer(char** ppBuffer, unsigned int nbufSize)
{
	_ASSERT(*ppBuffer);
	if (*ppBuffer)	// allow a zero size buffer on input, so long as not a nullptr
	{
		char* pExpanded = new char[nbufSize + 1];
		memcpy(pExpanded, *ppBuffer, nbufSize);
		delete* ppBuffer;
		*ppBuffer = pExpanded;
		(*ppBuffer)[nbufSize] = 0;
		return nbufSize + 1;
	}
	return 0;
}

bool DownloadText(const WCHAR* pwszUserAgent,
	const WCHAR* pwszUrl,
	char** ppszNewBuffer,
	int* pnReturnedBufferSizeInBytes)
{
	if (DownloadBinaryFile(pwszUserAgent, pwszUrl, ppszNewBuffer, pnReturnedBufferSizeInBytes))
	{
		// expand by +1 bytes to accomodate null terminator
		AddNullTerminatorToBuffer(ppszNewBuffer, *pnReturnedBufferSizeInBytes);
		return true;
	}
	return false;
}

bool DownloadBinaryFile(const WCHAR* pwszUserAgent,
	const WCHAR* pwszUrl,
	char** ppBuffer,
	int* pnReturnedBufferSizeInBytes)
{
	_ASSERT(pnReturnedBufferSizeInBytes && ppBuffer);
	HINTERNET hInternetFile;
	HINTERNET hConnection;

	DEBUG_PRINT(L"DownloadFile Agent: %s URL: %s\n", pwszUserAgent, pwszUrl);

	if (!(hConnection = InternetOpenW(pwszUserAgent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0)))
	{
		DEBUG_PRINT(L"\n ERROR: Open Internet 1");
		return false;
	}

	if (!(hInternetFile = InternetOpenUrlW(hConnection, pwszUrl, NULL, 0, INTERNET_FLAG_DONT_CACHE, 0)))
	{
		DEBUG_PRINT(L"ERROR - AS_ERROR_CONNECT_GENERAL");
		InternetCloseHandle(hConnection);
		return false;
	}

	static const int DL_BLOCK_SIZE = 4096;

	*ppBuffer = new char[DL_BLOCK_SIZE];
	int nTotalBytesRead = 0;
	DWORD dwBytesRead;

	while (!(InternetReadFile(hInternetFile,
		(*ppBuffer) + nTotalBytesRead,
		DL_BLOCK_SIZE,
		&dwBytesRead)
		&&
		!dwBytesRead))
	{
		nTotalBytesRead += dwBytesRead;

		//		
		// reallocate the buffer, expanding by block size
		//
		char* pExpanded = new char[nTotalBytesRead + DL_BLOCK_SIZE];
		memcpy(pExpanded, *ppBuffer, nTotalBytesRead);
		delete* ppBuffer;
		*ppBuffer = pExpanded;
	}

	InternetCloseHandle(hInternetFile);
	InternetCloseHandle(hConnection);

	if (pnReturnedBufferSizeInBytes)
	{
		*pnReturnedBufferSizeInBytes = nTotalBytesRead;
	}

	if (nTotalBytesRead)
	{
		DEBUG_PRINT(L"Returned download size of %d bytes", nTotalBytesRead);
	}
	else
	{
		if (*ppBuffer)
		{
			delete* ppBuffer;
			*ppBuffer = nullptr;
		}
		return false;
	}

	return true;
}

unsigned long TextVersionToULONG(const WCHAR* pwszTextVersion)
{
	_ASSERT(pwszTextVersion);
	if (!pwszTextVersion || wcslen(pwszTextVersion) < 7)
	{
		return 0;
	}
	unsigned int n1 = 0, n2 = 0, n3 = 0, n4 = 0;
	swscanf_s(pwszTextVersion, L"%u.%u.%u.%u", &n1, &n2, &n3, &n4);
	return (n1 << 24 | n2 << 16 | n3 << 8 | n4);
}