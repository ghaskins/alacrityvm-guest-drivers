REM @echo off

if "%1"=="" goto help
if "%2"=="" goto help
if "%3"=="" goto help
if "%4"=="" goto help
if "%5"=="" goto help

if not exist %6\dist goto errwinvmdp
cd %6\dist

if not exist vmdp_stage goto errvmdpstage

if not exist %5 goto makedirs
del /s /f /q %5\%4\%3

:makedirs
echo "making dirs %5"
if not exist %5 mkdir %5
if not exist %5\%4 mkdir %5\%4
if not exist %5\%4\%3 mkdir %5\%4\%3

if "%3" == "x86" goto x86
goto x64
:x86
copy vmdp_base\%1-32%2\*.* %5\%4\%3
copy vmdp_stage\%1-32%2\*.* %5\%4\%3
goto pdb
:x64
copy vmdp_base\%1-64%2\*.* %5\%4\%3
copy vmdp_stage\%1-64%2\*.* %5\%4\%3

:pdb
if "%2"=="chk" goto end
del %5\%4\%3\*.pdb
goto end

:help
echo "copywinpv.bat <platform> <release> <arch> <dir> <version_name> [<dir>]"
goto end

:errwinvmdp
echo "%6\dist does not exist."
goto end

:errvmdpstage
echo "%6\dist\vmdp_stage does not exist."
goto end

:end
