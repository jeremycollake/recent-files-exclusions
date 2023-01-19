#pragma once

#include <windows.h>
#include <vector>
#include <string>

#include "DebugOut.h"

class ListSerializer
{
public:
	int LoadListFromFile(_In_ const std::wstring& strFile, _Out_ std::vector<std::wstring>& vStrings)
	{
		HANDLE hFile = CreateFile(strFile.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			DWORD dwSizeHigh = 0;
			DWORD dwSizeLow = GetFileSize(hFile, &dwSizeHigh);
			if (dwSizeLow != INVALID_FILE_SIZE)
			{
				DWORD dwBytesRead = 0;
				std::vector<wchar_t> buffer(dwSizeLow);
				if (ReadFile(hFile, buffer.data(), dwSizeLow, &dwBytesRead, NULL))
				{
					for (unsigned long nPos = 0, nLineStart = 0; nPos < dwSizeLow; nPos++)
					{
						if (buffer[nPos] == L'\n')
						{
							auto strLine = std::wstring(buffer.begin() + nLineStart, buffer.begin() + nPos);
							// CR (\r) may or may not exist depending on how file was saved, strip occurrences
							strLine.erase(std::remove(strLine.begin(), strLine.end(), L'\r'), strLine.end());
							DEBUG_PRINT(L"Adding exclusion %s", strLine.c_str());
							vStrings.push_back(strLine);
							nLineStart = nPos + 1;
						}
					}
				}
			}
			CloseHandle(hFile);
		}
		return static_cast<int>(vStrings.size());
	}
	int SaveListToFile(_In_ const std::wstring& strFile, _In_ std::vector<std::wstring> vStrings)
	{
		int nRet = -1;
		HANDLE hFile = CreateFile(strFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			DWORD dwBytesWritten = 0;
			std::wstring strData;
			for (auto str : vStrings)
			{
				strData += str + L"\r\n";
			}
			if (WriteFile(hFile, strData.c_str(), static_cast<DWORD>(strData.size() * sizeof(wchar_t)), &dwBytesWritten, NULL))
			{
				nRet = static_cast<int>(vStrings.size());
			}
			CloseHandle(hFile);
		}
		return nRet;
	}
};