!define MUI_LANGDLL_ALLLANGUAGES

!include "MUI2.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "myincludes.nsh"
!include "LogicLib.nsh"

SetCompressor /Solid lzma
SetCompressorDictSize 32

; version information
!define Version_File "1.0.0.0"
!define Version_Text "1.0.0.0"
!define Version_Major "1"
!define Version_Minor "0"

;!define beta_version

!ifdef beta_version    
    BrandingText "Recent Files Exclusions - ${Version_Text} BETA - Built on ${__DATE__} at ${__TIME__} - (c)2023 Bitsum LLC"    
!else
	BrandingText "Recent Files Exclusions - ${Version_Text} - Built on ${__DATE__} at ${__TIME__} - (c)2023 Bitsum LLC"
!endif

;
; UAC plug-in requires the installer itself start with user level permissions
; it then launches another isntance with admin permissions
;
RequestExecutionLevel admin

; The name of the installer
Name "RecentFilesExclusions"	

Icon "..\images\icon1.ico"					; must contain 32x32@16bpp icon
!define MUI_ICON "..\images\icon1.ico"		; must contain 32x32@16bpp icon
;!define MUI_HEADERIMAGE_BITMAP "..\images\instheader.bmp"

XPStyle On
InstProgressFlags smooth colored

; The file to write
OutFile ".\x64\Release\RecentFilesExclusionsSetup.exe"

InstallDir "$PROGRAMFILES64\RecentFilesExclusions"

InstallDirRegKey HKCU Software\RecentFilesExclusions "Install_Dir"  ; leave HKCU to prevent UAC registry virtualization
LicenseBkColor /windows
AutoCloseWindow true

!define MUI_LICENSEPAGE_BGCOLOR /grey
!define MUI_ABORTWARNING  

!define MUI_LANGDLL_REGISTRY_ROOT "HKCU"                  ; leave HKCU to prevent UAC registry virtualization
!define MUI_LANGDLL_REGISTRY_KEY "Software\RecentFilesExclusions"
!define MUI_LANGDLL_REGISTRY_VALUENAME "InstallerLanguage"
!define MUI_LANGDLL_ALWAYSSHOW

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; sections
;
;!insertmacro MUI_PAGE_WELCOME					;; todo: do we really want this?
!insertmacro MUI_PAGE_LICENSE $(myLicenseData)
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

!insertmacro MUI_RESERVEFILE_LANGDLL

LicenseLangString myLicenseData ${LANG_ENGLISH} "languages\english\eula.txt"
!include "languages\english\installer_strings.txt"

; ======================================= 
; Globals (all are globals in NSIS)                                
;

!include FileFunc.nsh
!insertmacro GetParameters
!insertmacro GetOptions

Var GIVEN_LANGUAGE

; ==================================================================================
;
; .onInit splash screen display (using advsplash plug-in)
;
Function .onInit       
  
  ${IfNot} ${AtLeastWinVista}
    MessageBox MB_ICONEXCLAMATION $(OS_Not_Supported)
    Quit
  ${EndIf}  

  WriteRegStr "HKCU" "Software\RecentFilesExclusions" "Install_Dir" $0
  WriteRegStr "HKLM" "Software\RecentFilesExclusions" "Install_Dir" $0
  
  IfSilent 0 ask_for_language     
    ; get parameters and fill variables if silent install
    ${GetParameters} $R0
    ClearErrors
    ${GetOptions} $R0 /language= $0	
    StrCpy $LANGUAGE $0    
	StrCpy $GIVEN_LANGUAGE $LANGUAGE   
	;; verified language is correct here
    StrLen $0 $LANGUAGE
    IntCmp $0 0 0 have_default_lang have_default_lang
have_default_lang: 
    goto skip_language_query   
    
ask_for_language:  
  ; make sure installer is in foreground (can be lost during x64 build download)
  IfSilent +2
    BringToFront  
        
  IfSilent skip_language_query     
	  !insertmacro MUI_LANGDLL_DISPLAY  
skip_language_query:
FunctionEnd

; ==================================================================================
;
; main install section
;
Section $(SecCore) DESC_SecCore
  SectionIn RO
 
  ${DisableX64FSRedirection}
        
  CreateDirectory $INSTDIR
  SetOutPath $INSTDIR
  
  ; remove any old Hidden attribute on our install folder
  SetFileAttributes "$INSTDIR" NORMAL        
   
  StrLen $0 $GIVEN_LANGUAGE
    IntCmp $0 0 no_lang_on_cmd_line
	StrCpy $LANGUAGE $GIVEN_LANGUAGE
  no_lang_on_cmd_line: 
    
  SetOutPath $INSTDIR   
     	
  ; close existing instance if running
  IfFileExists "$INSTDIR\RecentFilesExclusions.exe" 0 +1
    ExecWait '"$INSTDIR\RecentFilesExclusions.exe" -close"'    
     
  SetOverwrite on    
  File "..\x64\release\RecentFilesExclusions.exe"  
    
  WriteRegStr HKCU "SOFTWARE\RecentFilesExclusions" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "SOFTWARE\RecentFilesExclusions" "Install_Dir" "$INSTDIR"
  
  ;
  ; wipe start menu slate
  ;  
  SetShellVarContext all        
  Delete "$SMPROGRAMS\RecentFilesExclusions\*"    
  RMDir /r '$SMPROGRAMS\RecentFilesExclusions\'     ; must use single quotes
     
  SetShellVarContext current
  Delete "$SMPROGRAMS\RecentFilesExclusions\*"
  RMDir /r '$SMPROGRAMS\RecentFilesExclusions\'    ; must use single quotes
   
  ; -------------------------------------------------------------------
  ;
  ; Create start menu shortcuts
  ; 
  SetShellVarContext all                       ; TODO: Add installer option to choose between current and all users (#7)
  CreateDirectory "$SMPROGRAMS\RecentFilesExclusions"  
  CreateShortCut "$SMPROGRAMS\RecentFilesExclusions\Recent Items Exclusions.lnk" "$INSTDIR\RecentFilesExclusions.exe" "" "$INSTDIR\RecentFilesExclusions.exe" 0 SW_SHOWNORMAL "" "Launch Recent tems Exclusions"
  
  ; install the startup task
  ExecWait '"$INSTDIR\RecentFilesExclusions.exe" -install'

  ;
  ; Write the uninstall keys for Windows
  ;
  ; write version info 
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "DisplayVersion" ${Version_Text}
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "Version" ${Version_File}
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "VersionMajor" ${Version_Major}
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "VersionMinor" ${Version_Minor}
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "Publisher" "Bitsum"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "URLInfoAbout" "https://bitsum.com/apps/RecentFilesExclusions"  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "HelpLink" "https://bitsum.com/apps/RecentFilesExclusions"  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "InstallLocation" "$INSTDIR"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "DisplayIcon" '"$INSTDIR\RecentFilesExclusions.exe"'    
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "DisplayName" "RecentFilesExclusions"  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
   
SectionEnd

; ==================================================================================
;
;  Launch after install 
;
Section "Launch RecentFilesExclusions" SecLaunch
    Exec '"$INSTDIR\RecentFilesExclusions.exe" --show'
SectionEnd
; ==================================================================================

; ==================================================================================
; Main uninstall
;
Section uninstall
 
  SetShellVarContext all           
  Delete "$SMPROGRAMS\RecentFilesExclusions\*"  
  RMDir /r "$SMPROGRAMS\RecentFilesExclusions"   
         
  ExecWait '"$INSTDIR\RecentFilesExclusions.exe" -close"'	    		  ; close any running instances
  ExecWait '"$INSTDIR\RecentFilesExclusions.exe" -uninstall"'			  ; remove startup locations
  
  DeleteRegKey HKLM "SOFTWARE\RecentFilesExclusions"
  DeleteRegKey HKCU "SOFTWARE\RecentFilesExclusions"
  DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions"
  DeleteRegKey HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\RecentFilesExclusions"

  ; deprecated keys
  DeleteRegKey HKLM "SOFTWARE\Recent Files Exclusions"
  DeleteRegKey HKCU "SOFTWARE\Recent Files Exclusions"
    
  Delete "$INSTDIR\*"   
  Delete "$SMPROGRAMS\RecentFilesExclusions\*"  
  RMDir /r "$INSTDIR"      

SectionEnd

Function un.onInit  
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd

Function .OnInstFailed
FunctionEnd
 
Function .OnInstSuccess
FunctionEnd

;Assign language strings to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${SecCore} $(DESC_SecCore)
!insertmacro MUI_DESCRIPTION_TEXT ${SecLaunch} $(DESC_SecLaunch)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Version Info block
;
;
VIProductVersion ${Version_File}
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "Recent Files Exclusions"
VIAddVersionKey /LANG=${LANG_ENGLISH} "Comments" "Recent Files Exclusions"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "Bitsum LLC"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "(c)2023 Bitsum LLC"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "Recent Files Exclusions Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${Version_Text}"
