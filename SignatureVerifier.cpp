#include <windows.h>
#include <softpub.h>

#include "RecentItemsExclusions.h"
#include "SignatureVerifier.h"
#include "../libcommon/libcommon/ResourceHelpers.h"

extern RecentItemsExclusions g_RecentItemsExclusionsApp;	// app globals and config

// this function from MSDN example
LONG SignatureVerifier::VerifyEmbeddedSignature(__in LPCWSTR pwszSourceFile)
{
	LONG lStatus;

	// Initialize the WINTRUST_FILE_INFO structure.

	WINTRUST_FILE_INFO FileData;
	memset(&FileData, 0, sizeof(FileData));
	FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
	FileData.pcwszFilePath = pwszSourceFile;
	FileData.hFile = NULL;
	FileData.pgKnownSubject = NULL;

	/*
	WVTPolicyGUID specifies the policy to apply on the file
	WINTRUST_ACTION_GENERIC_VERIFY_V2 policy checks:

	1) The certificate used to sign the file chains up to a root
	certificate located in the trusted root certificate store. This
	implies that the identity of the publisher has been verified by
	a certification authority.

	2) In cases where user interface is displayed (which this example
	does not do), WinVerifyTrust will check for whether the
	end entity certificate is stored in the trusted publisher store,
	implying that the user trusts content from this publisher.

	3) The end entity certificate has sufficient permission to sign
	code, as indicated by the presence of a code signing EKU or no
	EKU.
	*/

	GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	WINTRUST_DATA WinTrustData;

	// Initialize the WinVerifyTrust input data structure.

	// Default all fields to 0.
	memset(&WinTrustData, 0, sizeof(WinTrustData));

	WinTrustData.cbStruct = sizeof(WinTrustData);

	// Use default code signing EKU.
	WinTrustData.pPolicyCallbackData = NULL;

	// No data to pass to SIP.
	WinTrustData.pSIPClientData = NULL;

	// Disable WVT UI.
	WinTrustData.dwUIChoice = WTD_UI_NONE;

	// No revocation checking.
	WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;

	// Verify an embedded signature on a file.
	WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;

	// Verify action.
	WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;

	// Verification sets this value.
	WinTrustData.hWVTStateData = NULL;

	// Not used.
	WinTrustData.pwszURLReference = NULL;

	// This is not applicable if there is no UI because it changes 
	// the UI to accommodate running applications instead of 
	// installing applications.
	WinTrustData.dwUIContext = 0;

	// Set pFile.
	WinTrustData.pFile = &FileData;

	// WinVerifyTrust verifies signatures as specified by the GUID 
	// and Wintrust_Data.
	lStatus = WinVerifyTrust(
		NULL,
		&WVTPolicyGUID,
		&WinTrustData);
	// lStatus now holds return

	// Any hWVTStateData must be released by a call with close.
	WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
	// release
	WinVerifyTrust(
		NULL,
		&WVTPolicyGUID,
		&WinTrustData);

	return lStatus;
}

std::wstring SignatureVerifier::TranslateWinVerifySigningStatusToMessage(__in const LONG lStatus)
{
	std::wstring wstrMessage;

	switch (lStatus)
	{
	case ERROR_SUCCESS:
		/*
		Signed file:
			- Hash that represents the subject is trusted.

			- Trusted publisher without any verification errors.

			- UI was disabled in dwUIChoice. No publisher or
				time stamp chain errors.

			- UI was enabled in dwUIChoice and the user clicked
				"Yes" when asked to install and run the signed
				subject.
		*/
		wstrMessage = ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_SIGNED_AND_VERIFIED);
		break;

	case TRUST_E_NOSIGNATURE:
	{
		// The file was not signed or had a signature 
		// that was not valid.

		// Getthe reason for no signature.		
		DWORD dwLastError = GetLastError();
		if (TRUST_E_NOSIGNATURE == dwLastError ||
			TRUST_E_SUBJECT_FORM_UNKNOWN == dwLastError ||
			TRUST_E_PROVIDER_UNKNOWN == dwLastError)
		{
			wstrMessage = ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_SIGNED_NOT_SIGNED);
		}
		else
		{
			// The signature was not valid or there was an error 
			// opening the file.
			ATL::CString csStr;
			csStr.Format(ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_SIGNED_ERROR_UNKNOWN_FMT).c_str(), dwLastError, lStatus);
			wstrMessage = csStr;
		}
	}
	break;

	case TRUST_E_EXPLICIT_DISTRUST:
		// The hash that represents the subject or the publisher 
		// is not allowed by the admin or user.
		wstrMessage = ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_SIGNED_ERROR_DISALLOWED);

		break;

	case TRUST_E_SUBJECT_NOT_TRUSTED:
		// The user clicked "No" when asked to install and run.
		wstrMessage = ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_SIGNED_PRESENT_NOT_TRUSTED);
		break;

	case CRYPT_E_SECURITY_SETTINGS:
		wstrMessage = ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_SIGNED_NO_EXPLICIT);
		break;

	default:
		// The UI was disabled in dwUIChoice or the admin policy 
		// has disabled user trust. lStatus contains the 
		// publisher or time stamp chain error.
		ATL::CString csStr;
		csStr.Format(ResourceHelpers::LoadResourceString(g_RecentItemsExclusionsApp.hResourceModule, IDS_SIGNED_ERROR_UNKNOWN_FMT).c_str(), GetLastError(), lStatus);
		wstrMessage = csStr;
		break;
	}
	return wstrMessage;
}