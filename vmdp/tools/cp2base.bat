@echo off
REM cp2base.bat copies specified platform files to the stage area

set cpdir1=
set cpfile=
set cpos=

if "%1"=="" goto help
if "%1"=="all" goto all_setup
if "%1"=="eula" goto all_setup
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
set cpfile=%1
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
set cpos=%1
shift
goto dir_setup

:exe_setup
set cpfile=%1
shift
if "%1"=="" goto start
goto dir_setup

:all_setup
set cpfile=%1
shift
if "%1"=="" goto start
goto dir_setup

:dir_setup
set cpdir1=%1
goto start

:start

if not exist %cpdir1%\dist goto errwinvmdp

if not exist %cpdir1%\dist\vmdp_base mkdir %cpdir1%\dist\vmdp_base
if not exist %cpdir1%\dist\vmdp_base\main-fre mkdir %cpdir1%\dist\vmdp_base\main-fre
if not exist %cpdir1%\dist\vmdp_base\main-chk mkdir %cpdir1%\dist\vmdp_base\main-chk
if not exist %cpdir1%\dist\vmdp_base\w2k-32fre mkdir %cpdir1%\dist\vmdp_base\w2k-32fre
if not exist %cpdir1%\dist\vmdp_base\w2k-32chk mkdir %cpdir1%\dist\vmdp_base\w2k-32chk
if not exist %cpdir1%\dist\vmdp_base\wxp-32fre mkdir %cpdir1%\dist\vmdp_base\wxp-32fre
if not exist %cpdir1%\dist\vmdp_base\wxp-32chk mkdir %cpdir1%\dist\vmdp_base\wxp-32chk
if not exist %cpdir1%\dist\vmdp_base\w2k3-32fre mkdir %cpdir1%\dist\vmdp_base\w2k3-32fre
if not exist %cpdir1%\dist\vmdp_base\w2k3-32chk mkdir %cpdir1%\dist\vmdp_base\w2k3-32chk
if not exist %cpdir1%\dist\vmdp_base\w2k3-64fre mkdir %cpdir1%\dist\vmdp_base\w2k3-64fre
if not exist %cpdir1%\dist\vmdp_base\w2k3-64chk mkdir %cpdir1%\dist\vmdp_base\w2k3-64chk
if not exist %cpdir1%\dist\vmdp_base\w2k8-32fre mkdir %cpdir1%\dist\vmdp_base\w2k8-32fre
if not exist %cpdir1%\dist\vmdp_base\w2k8-32chk mkdir %cpdir1%\dist\vmdp_base\w2k8-32chk
if not exist %cpdir1%\dist\vmdp_base\w2k8-64fre mkdir %cpdir1%\dist\vmdp_base\w2k8-64fre
if not exist %cpdir1%\dist\vmdp_base\w2k8-64chk mkdir %cpdir1%\dist\vmdp_base\w2k8-64chk

if "%cpfile%"=="all" all
if "%cpfile%"=="xenbus" goto sys
if "%cpfile%"=="xenblk" goto sys
if "%cpfile%"=="xennet" goto sys
if "%cpfile%"=="setup" goto exe
if "%cpfile%"=="uninstall" goto exe
if "%cpfile%"=="pvctrl" goto exe
if "%cpfile%"=="xensvc" goto exe
if "%cpfile%"=="mainsetup" goto mainsetup
if "%cpfile%"=="eula" goto eula
goto help

:sys
if "%cpos%"=="" goto w2k
if "%cpos%"=="w2k" goto w2k
if "%cpos%"=="wxp" goto wxp
if "%cpos%"=="w2k3" goto w2k3_32
if "%cpos%"=="w2k3_32" goto w2k3_32
if "%cpos%"=="w2k3_64" goto w2k3_64
if "%cpos%"=="w2k8" goto w2k8_32
if "%cpos%"=="w2k8_32" goto w2k8_32
if "%cpos%"=="w2k8_64" goto w2k8_64
goto help


:all
xcopy /s /k /y /r %cpdir1%\dist\vmdp_stage %cpdir1%\dist\vmdp_base
goto eula

:w2k
copy /y %cpdir1%\dist\vmdp_stage\w2k-32fre\%cpfile%* %cpdir1%\dist\vmdp_base\w2k-32fre
copy /y %cpdir1%\dist\vmdp_stage\w2k-32chk\%cpfile%* %cpdir1%\dist\vmdp_base\w2k-32chk
if "%cpos%"=="w2k" goto end

:wxp
copy /y %cpdir1%\dist\vmdp_stage\wxp-32fre\%cpfile%* %cpdir1%\dist\vmdp_base\wxp-32fre
copy /y %cpdir1%\dist\vmdp_stage\wxp-32chk\%cpfile%* %cpdir1%\dist\vmdp_base\wxp-32chk
if "%cpos%"=="wxp" goto end

:w2k3_32
copy /y %cpdir1%\dist\vmdp_stage\w2k3-32fre\%cpfile%* %cpdir1%\dist\vmdp_base\w2k3-32fre
copy /y %cpdir1%\dist\vmdp_stage\w2k3-32chk\%cpfile%* %cpdir1%\dist\vmdp_base\w2k3-32chk
if "%cpos%"=="w2k3_32" goto end

:w2k3_64
copy /y %cpdir1%\dist\vmdp_stage\w2k3-64fre\%cpfile%* %cpdir1%\dist\vmdp_base\w2k3-64fre
copy /y %cpdir1%\dist\vmdp_stage\w2k3-64chk\%cpfile%* %cpdir1%\dist\vmdp_base\w2k3-64chk
if "%cpos%"=="w2k3" goto end
if "%cpos%"=="w2k3_64" goto end

:w2k8_32
copy /y %cpdir1%\dist\vmdp_stage\w2k8-32fre\%cpfile%* %cpdir1%\dist\vmdp_base\w2k8-32fre
copy /y %cpdir1%\dist\vmdp_stage\w2k8-32chk\%cpfile%* %cpdir1%\dist\vmdp_base\w2k8-32chk
if "%cpos%"=="w2k8_32" goto end

:w2k8_64
copy /y %cpdir1%\dist\vmdp_stage\w2k8-64fre\%cpfile%* %cpdir1%\dist\vmdp_base\w2k8-64fre
copy /y %cpdir1%\dist\vmdp_stage\w2k8-64chk\%cpfile%* %cpdir1%\dist\vmdp_base\w2k8-64chk
goto end

:exe
if not "%cpos%"=="" goto help
copy /y %cpdir1%\dist\vmdp_stage\w2k-32fre\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k-32fre
copy /y %cpdir1%\dist\vmdp_stage\wxp-32fre\%cpfile%.exe %cpdir1%\dist\vmdp_base\wxp-32fre
copy /y %cpdir1%\dist\vmdp_stage\w2k3-32fre\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k3-32fre
copy /y %cpdir1%\dist\vmdp_stage\w2k3-64fre\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k3-64fre
copy /y %cpdir1%\dist\vmdp_stage\w2k8-32fre\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k8-32fre
copy /y %cpdir1%\dist\vmdp_stage\w2k8-64fre\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k8-64fre

copy /y %cpdir1%\dist\vmdp_stage\w2k-32chk\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k-32chk
copy /y %cpdir1%\dist\vmdp_stage\wxp-32chk\%cpfile%.exe %cpdir1%\dist\vmdp_base\wxp-32chk
copy /y %cpdir1%\dist\vmdp_stage\w2k3-32chk\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k3-32chk
copy /y %cpdir1%\dist\vmdp_stage\w2k3-64chk\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k3-64chk
copy /y %cpdir1%\dist\vmdp_stage\w2k8-32chk\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k8-32chk
copy /y %cpdir1%\dist\vmdp_stage\w2k8-64chk\%cpfile%.exe %cpdir1%\dist\vmdp_base\w2k8-64chk
goto end

:mainsetup
copy /y %cpdir1%\dist\vmdp_stage\main-fre\setup.exe %cpdir1%\dist\vmdp_base\main-fre
copy /y %cpdir1%\dist\vmdp_stage\main-chk\setup.exe %cpdir1%\dist\vmdp_base\main-chk
goto end

:eula
xcopy /s /k /y /r %cpdir1%\dist\built\eula %cpdir1%\dist\vmdp_base\eula
goto end

:help
echo.
echo cp2base.bat copies specified platform files from stage to the base area
echo.
echo "syntax: cp2base all | <sys [platform]> | <exe> | eula [<dir>]"
echo "If all is specified, all files in stage will be copied to base."
echo "<sys> can be: xenbus, xenblk, xennet"
echo "[platform] is optional for <sys> and can be:"
echo "    w2k, wxp, w2k3, w2k3_32, w2k3_64, w2k8, w2k8_32, w2k8_64"
echo "<exe> can be: setup, uninstall, pvctrl, xensvc mainsetup"
echo.

echo example: cp2base all
echo example: cp2base xenbus
echo example: cp2base xenbus w2k3_32
echo example: cp2base setup
echo.
goto end

:errwinvmdp
echo "%cpdir1%\dist does not exist."
goto end

:end
set cpdir1=
set cpfile=
set cpos=
