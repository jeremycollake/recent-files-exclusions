@echo off
IF NOT [%1]==[] GOTO HAVE_ARG
echo ERROR: Must supply version as arg1 (e.g. 1.1.1.1)
exit /b 0
:HAVE_ARG
echo Uploading static link ...
scp .\installer\x64\release\RecentFilesExclusionsSetup.exe jeremy@az.bitsum.com:/var/www/vhosts/bitsum.com/files/RecentFilesExclusionsSetup.exe
echo Uploading versioned link ...
ssh jeremy@az.bitsum.com "mkdir -p /var/www/vhosts/bitsum.com/files/recentfilesexclusions/%1"
scp .\installer\x64\release\RecentFilesExclusionsSetup.exe jeremy@az.bitsum.com:/var/www/vhosts/bitsum.com/files/recentfilesexclusions/%1/RecentFilesExclusionsSetup.exe