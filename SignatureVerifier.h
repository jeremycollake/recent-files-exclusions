#pragma once

#include <windows.h>
#include <WinTrust.h>

#include <string>

#pragma comment (lib, "wintrust")

class SignatureVerifier
{
public:
	// this function from MSDN example
	LONG VerifyEmbeddedSignature(__in LPCWSTR pwszSourceFile);
	std::wstring TranslateWinVerifySigningStatusToMessage(__in const LONG lStatus);
};