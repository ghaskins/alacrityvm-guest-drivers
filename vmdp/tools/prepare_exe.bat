@echo off

set dir1=
set rsltname=
set preserveit=

if "%1"=="" goto help
set rsltname=%1
shift

if not "%1"=="preserve" goto dir_setup
set preserveit=%1
shift

:dir_setup
set dir1=%1

:start
cd %dir1%\dist
call %dir1%\vmdp\tools\make_exe_dirs.bat %rsltname% %dir1%

copy built\dpinst\dpinst.xml %rsltname%\dpinst
copy built\dpinst\dpinst.xml %rsltname%chk\dpinst

copy built\dpinst\x86\*.* %rsltname%\dpinst\x86
copy built\dpinst\x86\*.* %rsltname%chk\dpinst\x86

copy built\doc\README.txt %rsltname%
copy built\doc\README.txt %rsltname%chk

copy built\dpinst\x64\*.* %rsltname%\dpinst\x64
copy built\dpinst\x64\*.* %rsltname%chk\dpinst\x64

xcopy /s /k /y /r vmdp_base\eula %rsltname%\eula
xcopy /s /k /y /r vmdp_base\eula %rsltname%chk\eula

copy vmdp_base\main-fre\setup.exe %rsltname%
copy vmdp_base\main-chk\setup.exe %rsltname%chk

REM overwite with any newer files.
copy vmdp_stage\main-fre\setup.exe %rsltname%
copy vmdp_stage\main-chk\setup.exe %rsltname%chk

call %dir1%\vmdp\tools\cpwinpv.bat w2k fre x86 win2000 %rsltname% %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat wxp fre x86 winxp %rsltname% %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat w2k3 fre x86 win2003 %rsltname% %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat w2k3 fre x64 win2003 %rsltname% %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat w2k8 fre x86 win2008 %rsltname% %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat w2k8 fre x64 win2008 %rsltname% %dir1% 

REM Now do the check builds
call %dir1%\vmdp\tools\cpwinpv.bat w2k chk x86 win2000 %rsltname%chk %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat wxp chk x86 winxp %rsltname%chk %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat w2k3 chk x86 win2003 %rsltname%chk %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat w2k3 chk x64 win2003 %rsltname%chk %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat w2k8 chk x86 win2008 %rsltname%chk %dir1% 
call %dir1%\vmdp\tools\cpwinpv.bat w2k8 chk x64 win2008 %rsltname%chk %dir1% 

REM Now build the zipped exes
if exist %rsltname% del %rsltname%.exe
if exist %rsltname%chk del %rsltname%chk.exe
"\Program Files\7-Zip\7z.exe" a -sfx %rsltname%.exe %rsltname%
"\Program Files\7-Zip\7z.exe" a -sfx %rsltname%chk.exe %rsltname%chk

REM save the source files
call %dir1%\vmdp\tools\savesrc.bat %rsltname% %preserveit% %dir1%

if "%preserveit%"=="" goto cleanup
goto end
:cleanup
rmdir /s /q %rsltname%
rmdir /s /q %rsltname%chk
rem cd %dir1%\vmdp\tools
goto end

:help
echo "prepare_exe.bat <file_name> [preserve] [<dir1>]"
echo "prepare_exe.bat VMDP-WIN-20090415 [preserve] [<vmdpwin>]"

:end
set dir1=
set rsltname=
set preserveit=
