@echo off

set s_dir1=
set s_rsltname=
set s_preserveit=

if "%1"=="" goto help
set s_rsltname=%1
shift

if not "%1"=="preserve" goto dir_setup
set s_preserveit=%1
shift

:dir_setup
set s_dir1=%1

:start
if not exist %s_dir1%\dist\%s_rsltname%src mkdir %s_dir1%\dist\%s_rsltname%src
copy %s_dir1%\vmdp\*bat %s_dir1%\dist\%s_rsltname%src
copy %s_dir1%\vmdp\dirs %s_dir1%\dist\%s_rsltname%src

if not exist %s_dir1%\dist\%s_rsltname%src\include mkdir %s_dir1%\dist\%s_rsltname%src\include
copy %s_dir1%\vmdp\include\* %s_dir1%\dist\%s_rsltname%src\include

if not exist %s_dir1%\dist\%s_rsltname%src\include\asm mkdir %s_dir1%\dist\%s_rsltname%src\include\asm
copy %s_dir1%\vmdp\include\asm\* %s_dir1%\dist\%s_rsltname%src\include\asm

if not exist %s_dir1%\dist\%s_rsltname%src\include\xen mkdir %s_dir1%\dist\%s_rsltname%src\include\xen
if not exist %s_dir1%\dist\%s_rsltname%src\include\xen\public mkdir %s_dir1%\dist\%s_rsltname%src\include\xen\public
copy %s_dir1%\vmdp\include\xen\public\* %s_dir1%\dist\%s_rsltname%src\include\xen\public

if not exist %s_dir1%\dist\%s_rsltname%src\include\xen\public\arch-x86 mkdir %s_dir1%\dist\%s_rsltname%src\include\xen\public\arch-x86
copy %s_dir1%\vmdp\include\xen\public\arch-x86\* %s_dir1%\dist\%s_rsltname%src\include\xen\public\arch-x86

if not exist %s_dir1%\dist\%s_rsltname%src\include\xen\public\hvm mkdir %s_dir1%\dist\%s_rsltname%src\include\xen\public\hvm
copy %s_dir1%\vmdp\include\xen\public\hvm\* %s_dir1%\dist\%s_rsltname%src\include\xen\public\hvm

if not exist %s_dir1%\dist\%s_rsltname%src\include\xen\public\io mkdir %s_dir1%\dist\%s_rsltname%src\include\xen\public\io
copy %s_dir1%\vmdp\include\xen\public\io\* %s_dir1%\dist\%s_rsltname%src\include\xen\public\io

if not exist %s_dir1%\dist\%s_rsltname%src\inf mkdir %s_dir1%\dist\%s_rsltname%src\inf
copy %s_dir1%\vmdp\inf\* %s_dir1%\dist\%s_rsltname%src\inf
if not exist %s_dir1%\dist\%s_rsltname%src\inf\win2k mkdir %s_dir1%\dist\%s_rsltname%src\inf\win2k
copy %s_dir1%\vmdp\inf\win2k\* %s_dir1%\dist\%s_rsltname%src\inf\win2k

if not exist %s_dir1%\dist\%s_rsltname%src\mainsetup mkdir %s_dir1%\dist\%s_rsltname%src\mainsetup
copy %s_dir1%\vmdp\mainsetup\* %s_dir1%\dist\%s_rsltname%src\mainsetup

if not exist %s_dir1%\dist\%s_rsltname%src\pvctrl mkdir %s_dir1%\dist\%s_rsltname%src\pvctrl
copy %s_dir1%\vmdp\pvctrl\* %s_dir1%\dist\%s_rsltname%src\pvctrl

if not exist %s_dir1%\dist\%s_rsltname%src\setup mkdir %s_dir1%\dist\%s_rsltname%src\setup
copy %s_dir1%\vmdp\setup\* %s_dir1%\dist\%s_rsltname%src\setup

if not exist %s_dir1%\dist\%s_rsltname%src\setup\eula mkdir %s_dir1%\dist\%s_rsltname%src\setup\eula
xcopy /s /k /y /r %s_dir1%\vmdp\setup\eula %s_dir1%\dist\%s_rsltname%src\setup\eula

if not exist %s_dir1%\dist\%s_rsltname%src\redist mkdir %s_dir1%\dist\%s_rsltname%src\redist
xcopy /s /k /y /r %s_dir1%\vmdp\redist %s_dir1%\dist\%s_rsltname%src\redist

if not exist %s_dir1%\dist\%s_rsltname%src\tools mkdir %s_dir1%\dist\%s_rsltname%src\tools
copy %s_dir1%\vmdp\tools\* %s_dir1%\dist\%s_rsltname%src\tools
if not exist %s_dir1%\dist\%s_rsltname%src\tools\driverpack mkdir %s_dir1%\dist\%s_rsltname%src\tools\driverpack
copy %s_dir1%\vmdp\tools\driverpack\* %s_dir1%\dist\%s_rsltname%src\tools\driverpack

if not exist %s_dir1%\dist\%s_rsltname%src\uninstall mkdir %s_dir1%\dist\%s_rsltname%src\uninstall
copy %s_dir1%\vmdp\uninstall\* %s_dir1%\dist\%s_rsltname%src\uninstall

if not exist %s_dir1%\dist\%s_rsltname%src\xenblk mkdir %s_dir1%\dist\%s_rsltname%src\xenblk
copy %s_dir1%\vmdp\xenblk\* %s_dir1%\dist\%s_rsltname%src\xenblk

if not exist %s_dir1%\dist\%s_rsltname%src\xenbus mkdir %s_dir1%\dist\%s_rsltname%src\xenbus
copy %s_dir1%\vmdp\xenbus\* %s_dir1%\dist\%s_rsltname%src\xenbus
if not exist %s_dir1%\dist\%s_rsltname%src\xenbus\amd64 mkdir %s_dir1%\dist\%s_rsltname%src\xenbus\amd64
copy %s_dir1%\vmdp\xenbus\amd64\* %s_dir1%\dist\%s_rsltname%src\xenbus\amd64

if not exist %s_dir1%\dist\%s_rsltname%src\xennet mkdir %s_dir1%\dist\%s_rsltname%src\xennet
copy %s_dir1%\vmdp\xennet\* %s_dir1%\dist\%s_rsltname%src\xennet

if not exist %s_dir1%\dist\%s_rsltname%src\xensvc mkdir %s_dir1%\dist\%s_rsltname%src\xensvc
copy %s_dir1%\vmdp\xensvc\* %s_dir1%\dist\%s_rsltname%src\xensvc

if not exist %s_dir1%\dist\%s_rsltname%src\pdb mkdir %s_dir1%\dist\%s_rsltname%src\pdb
if not exist %s_dir1%\dist\%s_rsltname%src\pdb\w2k-32fre mkdir %s_dir1%\dist\%s_rsltname%src\pdb\w2k-32fre
copy %s_dir1%\dist\vmdp_base\w2k-32fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k-32fre
copy %s_dir1%\dist\vmdp_stage\w2k-32fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k-32fre

if not exist %s_dir1%\dist\%s_rsltname%src\pdb\wxp-32fre mkdir %s_dir1%\dist\%s_rsltname%src\pdb\wxp-32fre
copy %s_dir1%\dist\vmdp_base\wxp-32fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\wxp-32fre
copy %s_dir1%\dist\vmdp_stage\wxp-32fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\wxp-32fre

if not exist %s_dir1%\dist\%s_rsltname%src\pdb\w2k3-32fre mkdir %s_dir1%\dist\%s_rsltname%src\pdb\w2k3-32fre
copy %s_dir1%\dist\vmdp_base\w2k3-32fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k3-32fre
copy %s_dir1%\dist\vmdp_stage\w2k3-32fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k3-32fre

if not exist %s_dir1%\dist\%s_rsltname%src\pdb\w2k3-64fre mkdir %s_dir1%\dist\%s_rsltname%src\pdb\w2k3-64fre
copy %s_dir1%\dist\vmdp_base\w2k3-64fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k3-64fre
copy %s_dir1%\dist\vmdp_stage\w2k3-64fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k3-64fre

if not exist %s_dir1%\dist\%s_rsltname%src\pdb\w2k8-32fre mkdir %s_dir1%\dist\%s_rsltname%src\pdb\w2k8-32fre
copy %s_dir1%\dist\vmdp_base\w2k8-32fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k8-32fre
copy %s_dir1%\dist\vmdp_stage\w2k8-32fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k8-32fre

if not exist %s_dir1%\dist\%s_rsltname%src\pdb\w2k8-64fre mkdir %s_dir1%\dist\%s_rsltname%src\pdb\w2k8-64fre
copy %s_dir1%\dist\vmdp_base\w2k8-64fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k8-64fre
copy %s_dir1%\dist\vmdp_stage\w2k8-64fre\*.pdb %s_dir1%\dist\%s_rsltname%src\pdb\w2k8-64fre

:zipit
REM Now zip it up
if exist %s_dir1%\dist\%s_rsltname%src.zip del %s_dir1%\dist\%s_rsltname%src.zip
"\Program Files\7-Zip\7z.exe" a %s_dir1%\dist\%s_rsltname%src.zip %s_dir1%\dist\%s_rsltname%src

if "%s_preserveit%"=="" goto cleanup
goto end
:cleanup
rmdir /s /q %s_dir1%\dist\%s_rsltname%src
goto end

goto end

:help
echo "savesrc.bat <file_name> [preserve] [<dir1>]"
echo "savesrc.bat VMDP-WIN-20090415 [preserve] [<vmdpwin>]"

:end
set s_dir1=
set s_rsltname=
set s_preserveit=
