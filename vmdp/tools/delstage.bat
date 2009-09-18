@echo off
REM delstage.bat deletes specified platform files from the staging area

set deldir1=
set delfile=
set delos=

if "%1"=="" goto help
if "%1"=="xenbus" goto sys_setup
if "%1"=="xenblk" goto sys_setup
if "%1"=="xennet" goto sys_setup
if "%1"=="setup" goto exe_setup
if "%1"=="uninstall" goto exe_setup
if "%1"=="pvctrl" goto exe_setup
if "%1"=="xensvc" goto exe_setup
if "%1"=="mainsetup" goto exe_setup
goto help

:sys_setup
set delfile=%1
shift
if "%1"=="" goto start
if "%1"=="w2k" goto os_setup
if "%1"=="wxp" goto os_setup
if "%1"=="w2k3" goto os_setup
if "%1"=="w2k3_32" goto os_setup
if "%1"=="w2k3_64" goto os_setup
if "%1"=="w2k8" goto os_setup
if "%1"=="w2k8_32" goto os_setup
if "%1"=="w2k8_64" goto os_setup
goto dir_setup

:os_setup
set delos=%1
shift
goto dir_setup

:exe_setup
set delfile=%1
shift
if "%1"=="" goto start
goto dir_setup

:dir_setup
set deldir1=%1
goto start

:start

if not exist %deldir1%\dist goto errwinvmdp

if "%delfile%"=="xenbus" goto sys
if "%delfile%"=="xenblk" goto sys
if "%delfile%"=="xennet" goto sys
if "%delfile%"=="setup" goto exe
if "%delfile%"=="uninstall" goto exe
if "%delfile%"=="pvctrl" goto exe
if "%delfile%"=="xensvc" goto exe
if "%delfile%"=="mainsetup" goto mainsetup
goto help

:sys
if "%delos%"=="" goto w2k
if "%delos%"=="w2k" goto w2k
if "%delos%"=="wxp" goto wxp
if "%delos%"=="w2k3" goto w2k3_32
if "%delos%"=="w2k3_32" goto w2k3_32
if "%delos%"=="w2k3_64" goto w2k3_64
if "%delos%"=="w2k8" goto w2k8_32
if "%delos%"=="w2k8_32" goto w2k8_32
if "%delos%"=="w2k8_64" goto w2k8_64
goto help

:w2k
del /f /q %deldir1%\vmdp\vmdp_stage\w2k-32fre\%delfile%*
del /f /q %deldir1%\vmdp\vmdp_stage\w2k-32chk\%delfile%*
if "%2"=="w2k" goto end

:wxp
del /f /q %deldir1%\vmdp\vmdp_stage\wxp-32fre\%delfile%*
del /f /q %deldir1%\vmdp\vmdp_stage\wxp-32chk\%delfile%*
if "%2"=="wxp" goto end

:w2k3_32
del /f /q %deldir1%\vmdp\vmdp_stage\w2k3-32fre\%delfile%*
del /f /q %deldir1%\vmdp\vmdp_stage\w2k3-32chk\%delfile%*
if "%2"=="w2k3_32" goto end

:w2k3_64
del /f /q %deldir1%\vmdp\vmdp_stage\w2k3-64fre\%delfile%*
del /f /q %deldir1%\vmdp\vmdp_stage\w2k3-64chk\%delfile%*
if "%2"=="w2k3" goto end
if "%2"=="w2k3_64" goto end

:w2k8_32
del /f /q %deldir1%\vmdp\vmdp_stage\w2k8-32fre\%delfile%*
del /f /q %deldir1%\vmdp\vmdp_stage\w2k8-32chk\%delfile%*
if "%2"=="w2k8_32" goto end

:w2k8_64
del /f /q %deldir1%\vmdp\vmdp_stage\w2k8-64fre\%delfile%*
del /f /q %deldir1%\vmdp\vmdp_stage\w2k8-64chk\%delfile%*
goto end

:exe
if not "%delos%"=="" goto help
del /f /q %deldir1%\vmdp\vmdp_stage\w2k-32fre\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\wxp-32fre\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\w2k3-32fre\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\w2k3-64fre\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\w2k8-32fre\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\w2k8-64fre\%delfile%.exe

del /f /q %deldir1%\vmdp\vmdp_stage\w2k-32chk\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\wxp-32chk\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\w2k3-32chk\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\w2k3-64chk\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\w2k8-32chk\%delfile%.exe
del /f /q %deldir1%\vmdp\vmdp_stage\w2k8-64chk\%delfile%.exe
goto end

:mainsetup
del /f /q %deldir1%\vmdp\vmdp_stage\main-fre\setup.exe
del /f /q %deldir1%\vmdp\vmdp_stage\main-chk\setup.exe
goto end

:help
echo.
echo delstage.bat deletes specified platform files from the stage area
echo.
echo "syntax: delstage <sys [platform]> | <exe> [<dir>]"
echo "<sys> can be: xenbus, xenblk, xennet"
echo "[platform] is optional for <sys> and can be:"
echo "    w2k, wxp, w2k3, w2k3_32, w2k3_64, w2k8, w2k8_32, w2k8_64"
echo "<exe> can be: setup, uninstall, pvctrl, xensvc mainsetup"
echo.

echo example: delstage xenbus
echo example: delstage xenbus w2k3_32
echo example: delstage setup
echo.
goto end

:errwinvmdp
echo "%deldir1%\vmdp does not exist."
goto end

:end
set deldir1=
set delfile=
set delos=
