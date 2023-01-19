#pragma once

#include <windows.h>
#include <vector>
#include <string>

INT_PTR WINAPI ListDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
bool SetListInDialog(const HWND hWndList, const std::vector<std::wstring>& vStrings);
bool GetListFromDialog(HWND hWndList, std::vector<std::wstring>& vStrings);
INT_PTR WINAPI NewEntryDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);