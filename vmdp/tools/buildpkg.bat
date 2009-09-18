@echo off
REM buildpkg.bat builds all of the windows paravirtual drivers,
REM copies them to the location specified, and zips up the sys files.
echo buildpkg.bat %1

if "%1"=="c" goto help
if "%1"=="z" goto help
if "%1"=="b" goto help

call %1\vmdp\tools\mkvmdpdirs.bat %1

if "%2"=="c" goto cp
if "%2"=="z" goto zip

call %1\vmdp\tools\builddrivers.bat %1
if "%2"=="b" goto bld

:cp
del /s /f /q %1\dist\sign\sys
del /s /f /q %1\dist\sign\cat
del /s /f /q %1\dist\sign\exe
rem del /s /f /q \vmdp
call %1\vmdp\tools\copy4sign.bat %1
call %1\vmdp\tools\cp_nonsign.bat %1

:zip
if not exist %1\dist\sign\winpv2sign.zip goto createzip
copy /y %1\dist\sign\*.zip %1\dist\prev_sign
del %1\dist\sign\*.zip
:createzip
"\Program Files\7-Zip\7z.exe" a %1\dist\sign\winpv2sign.zip %1\dist\sign\sys
"\Program Files\7-Zip\7z.exe" a %1\dist\sign\winpvexe2sign.zip %1\dist\sign\exe
goto end

:bld
call %1\vmdp\tools\cp_nonsign.bat %1
goto end

:help
echo.
echo buildpkg.bat builds all of the driver kit files in 
echo /%1/vmdp and optionally copies them.
echo.
echo "syntax: buildpkg [<dir>] [b | c | z ]"

echo example: buildpkg - Assumes \vmdp
echo example: buildpkg <dir> - Assumes <dir>\vmdp
echo optional parameters: b - builds, c - copyies and zips,	z -  zips
echo.

:end
cd %1\vmdp\tools
