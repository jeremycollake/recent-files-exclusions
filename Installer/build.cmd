@echo off
echo Signing modules ...
call bitsum-sign.bat "..\x64\release\RecentFilesExclusions.exe"

echo Building installer ...
"c:\dev\nsis\bin\makensis" recentitemsexclusions.nsi

rem sign installers
echo Signing installer ... 2=%2 3=%3
if exist ".\x64\release\RecentFilesExclusionsSetup.exe" (			
	call bitsum-sign.bat ".\x64\release\RecentFilesExclusionsSetup.exe"
	)