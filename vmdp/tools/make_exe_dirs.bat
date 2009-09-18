@echo off

if "%1"=="" goto help

if not exist %2\dist goto direrr
cd %2\dist

if not exist %1 mkdir %1
if not exist %1\dpinst mkdir %1\dpinst
if not exist %1\dpinst\x86 mkdir %1\dpinst\x86
if not exist %1\dpinst\x64 mkdir %1\dpinst\x64
if not exist %1\eula mkdir %1\eula
if not exist %1\win2000 mkdir %1\win2000
if not exist %1\win2000\x86 mkdir %1\win2000\x86
if not exist %1\winxp mkdir %1\winxp
if not exist %1\winxp\x86 mkdir %1\winxp\x86
rem if not exist %1\winxp\x64 mkdir %1\winxp\x64
if not exist %1\win2003 mkdir %1\win2003
if not exist %1\win2003\x86 mkdir %1\win2003\x86
if not exist %1\win2003\x64 mkdir %1\win2003\x64
if not exist %1\win2008 mkdir %1\win2008
if not exist %1\win2008\x86 mkdir %1\win2008\x86
if not exist %1\win2008\x64 mkdir %1\win2008\x64

if not exist %1chk mkdir %1chk
if not exist %1chk\dpinst mkdir %1chk\dpinst
if not exist %1chk\dpinst\x86 mkdir %1chk\dpinst\x86
if not exist %1chk\dpinst\x64 mkdir %1chk\dpinst\x64
if not exist %1chk\eula mkdir %1chk\eula
if not exist %1chk\win2000 mkdir %1chk\win2000
if not exist %1chk\win2000\x86 mkdir %1chk\win2000\x86
if not exist %1chk\winxp mkdir %1chk\winxp
if not exist %1chk\winxp\x86 mkdir %1chk\winxp\x86
rem if not exist %1chk\winxp\x64 mkdir %1chk\winxp\x64
if not exist %1chk\win2003 mkdir %1chk\win2003
if not exist %1chk\win2003\x86 mkdir %1chk\win2003\x86
if not exist %1chk\win2003\x64 mkdir %1chk\win2003\x64
if not exist %1chk\win2008 mkdir %1chk\win2008
if not exist %1chk\win2008\x86 mkdir %1chk\win2008\x86
if not exist %1chk\win2008\x64 mkdir %1chk\win2008\x64

del /s /f /q %1
del /s /f /q %1chk
goto end

:help 
echo "make_exe_dirs.bat <file_name> [<dir>]"
goto end

:direrr
echo "%2\dist does not exist."

:end

