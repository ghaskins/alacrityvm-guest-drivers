@echo off
REM This Batch file builds all of the windows paravirtual drivers for
REM all platforms and architectures

set start_path=%path%
:win2k-32bit-fre
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ w2k fre 
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:win2k-32bit-chk
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ w2k chk 
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:winxp-32bit-fre
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WXP fre
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:winxp-32bit-chk
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WXP chk
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:win2k3-32bit-fre
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WNET fre
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:win2k3-32bit-chk
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WNET chk
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:win2k3-64bit-fre
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WNET x64 fre
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:win2k3-64bit-chk
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WNET x64 chk
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:winvlh-32bit-fre
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WLH fre
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:winvlh-32bit-chk
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WLH chk
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:winvlh-64bit-fre
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WLH x64 fre
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr

:winvlh-64bit-chk
set DDKBUILDENV=
call \WinDDK\6001.18002\bin\setenv.bat \WinDDK\6001.18002\ WLH x64 chk
cd %1\vmdp
call buildpv.bat -cZ
path=%start_path%
if exist *.err goto builderr
goto end

:builderr
echo.
echo.
echo.
echo THE BUILD IS BROKEN!!  Please look for an error file in the winpvdrvs directory.
echo.
goto end

:help
echo.
echo builddrivers.bat builds all of the driver kit files in 
echo \[dir1]\[subdir2]\vmdp
echo.
echo "syntax: builddrivers.bat [<dir1>] [<subdir2>]"
echo example: builddrivers \vmdpwin \release
echo example: builddrivers \vmdpwin
echo example: builddrivers
echo.

:end
