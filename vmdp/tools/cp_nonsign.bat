@echo off
REM cp_nonsign copys all files in preparation for signing and packaging.

REM common copies
copy /y %1\vmdp\redist\x86\*.* %1\dist\built\dpinst\x86
copy /y %1\vmdp\redist\amd64\*.* %1\dist\built\dpinst\x64
copy /y %1\vmdp\setup\dpinst.xml %1\dist\built\dpinst
copy /y %1\vmdp\setup\suse.ico %1\dist\built\base
xcopy /s /k /y /r %1\vmdp\setup\eula %1\dist\built\eula


copy /y %1\vmdp\inf\*.cdf %1\dist\built\cdf

REM copy the documentation for the add-on product iso (the big iso)
copy /y %1\vmdp\setup\README.txt %1\dist\built\doc
copy /y %1\vmdp\setup\susedriversforlinux.pdf %1\dist\built\doc
copy /y %1\vmdp\setup\susedriversforlinux_html.zip %1\dist\built\doc
copy /y %1\vmdp\setup\susedriversforwindows.pdf %1\dist\built\doc
copy /y %1\vmdp\setup\susedriversforwindows_html.zip %1\dist\built\doc

REM :win2k-32bit-fre
copy /y %1\vmdp\inf\win2k\*.inf %1\dist\built\w2k-32fre


REM :win2k-32bit-chk
copy /y %1\vmdp\inf\win2k\*.inf %1\dist\built\w2k-32chk

REM:winxp-32bit-fre
copy /y %1\vmdp\inf\*.inf %1\dist\built\wxp-32fre

REM:winxp-32bit-chk
copy /y %1\vmdp\inf\*.inf %1\dist\built\wxp-32chk

REM:win2k3-32bit-fre
copy /y %1\vmdp\inf\*.inf %1\dist\built\w2k3-32fre

REM:win2k3-32bit-chk
copy /y %1\vmdp\inf\*.inf %1\dist\built\w2k3-32chk

REM:win2k3-64bit-fre
copy /y %1\vmdp\inf\*.inf %1\dist\built\w2k3-64fre

REM:win2k3-64bit-chk
copy /y %1\vmdp\inf\*.inf %1\dist\built\w2k3-64chk

REM:winvlh-32bit-fre
copy /y %1\vmdp\inf\*.inf %1\dist\built\w2k8-32fre

REM:winvlh-32bit-chk
copy /y %1\vmdp\inf\*.inf %1\dist\built\w2k8-32chk

REM:winvlh-64bit-fre
copy /y %1\vmdp\inf\*.inf %1\dist\built\w2k8-64fre

REM:winvlh-64bit-chk
copy /y %1\vmdp\inf\*.inf %1\dist\built\w2k8-64chk
goto end

:help
echo.
echo cp_nonsign.bat copys exe files and support files
echo.
echo "syntax: cp_nonsign [<dir>]"
echo example: cp_nonsign <dir>
echo example: cp_nonsign
echo.

:end
