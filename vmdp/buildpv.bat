echo off

attrib -r uninstall\xencleanup.c
attrib -r uninstall\xencleanup.h
copy setup\xencleanup.c uninstall
copy setup\xencleanup.h uninstall
attrib +r uninstall\xencleanup.c
attrib +r uninstall\xencleanup.h

if "%DDK_TARGET_OS%" == "Win2K"  goto Win2K
if "%DDK_TARGET_OS%" == "WinXP"  goto WinXP
if "%DDK_TARGET_OS%" == "WinNET" goto WinNET
if "%DDK_TARGET_OS%" == "WinLH"  goto WinLH
if "%DDK_TARGET_OS%" == "Win7"   goto Win7
goto end

:Win2K
copy xennet\src_files5 xennet\src_files
goto end

:WinXP
copy xennet\src_files5 xennet\src_files
goto end

:WinNET
copy xennet\src_files5 xennet\src_files
goto end

:WinLH
copy xennet\src_files6 xennet\src_files
goto end

:Win7
copy xennet\src_files6 xennet\src_files
goto end

:end
build %1
