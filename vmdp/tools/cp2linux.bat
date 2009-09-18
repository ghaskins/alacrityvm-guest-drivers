@echo off
REM cp2linux.bat copies specified platform files to linux

if "%1"=="" goto help
if "%2"=="" goto help

if "%1"=="xenbus" goto sys
if "%1"=="xenblk" goto sys
if "%1"=="xennet" goto sys
goto vmdp

:sys
REM W2k
pscp -p -pw novell \winvmdp\sign\sys\w2k-32fre\%1* root@%2:/root/driverpack/winpv_stage/w2k-32fre
pscp -p -pw novell \winvmdp\sign\cat\w2k-32fre\%1* root@%2:/root/driverpack/winpv_stage/w2k-32fre
pscp -p -pw novell \winvmdp\built\w2k-32fre\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k-32fre

pscp -p -pw novell \winvmdp\sign\sys\w2k-32chk\%1* root@%2:/root/driverpack/winpv_stage/w2k-32chk
pscp -p -pw novell \winvmdp\sign\cat\w2k-32chk\%1* root@%2:/root/driverpack/winpv_stage/w2k-32chk
pscp -p -pw novell \winvmdp\built\w2k-32chk\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k-32chk

REM WXP
pscp -p -pw novell \winvmdp\sign\sys\wxp-32fre\%1* root@%2:/root/driverpack/winpv_stage/wxp-32fre
pscp -p -pw novell \winvmdp\sign\cat\wxp-32fre\%1* root@%2:/root/driverpack/winpv_stage/wxp-32fre
pscp -p -pw novell \winvmdp\built\wxp-32fre\%1.pdb root@%2:/root/driverpack/winpv_stage/wxp-32fre

pscp -p -pw novell \winvmdp\sign\sys\wxp-32chk\%1* root@%2:/root/driverpack/winpv_stage/wxp-32chk
pscp -p -pw novell \winvmdp\sign\cat\wxp-32chk\%1* root@%2:/root/driverpack/winpv_stage/wxp-32chk
pscp -p -pw novell \winvmdp\built\wxp-32chk\%1.pdb root@%2:/root/driverpack/winpv_stage/wxp-32chk

REM W2k3 32
pscp -p -pw novell \winvmdp\sign\sys\w2k3-32fre\%1* root@%2:/root/driverpack/winpv_stage/w2k3-32fre
pscp -p -pw novell \winvmdp\sign\cat\w2k3-32fre\%1* root@%2:/root/driverpack/winpv_stage/w2k3-32fre
pscp -p -pw novell \winvmdp\built\w2k3-32fre\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k3-32fre

pscp -p -pw novell \winvmdp\sign\sys\w2k3-32chk\%1* root@%2:/root/driverpack/winpv_stage/w2k3-32chk
pscp -p -pw novell \winvmdp\sign\cat\w2k3-32chk\%1* root@%2:/root/driverpack/winpv_stage/w2k3-32chk
pscp -p -pw novell \winvmdp\built\w2k3-32chk\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k3-32chk

REM W2k3 64
pscp -p -pw novell \winvmdp\sign\sys\w2k3-64fre\%1* root@%2:/root/driverpack/winpv_stage/w2k3-64fre
pscp -p -pw novell \winvmdp\sign\cat\w2k3-64fre\%1* root@%2:/root/driverpack/winpv_stage/w2k3-64fre
pscp -p -pw novell \winvmdp\built\w2k3-64fre\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k3-64fre

pscp -p -pw novell \winvmdp\sign\sys\w2k3-64chk\%1* root@%2:/root/driverpack/winpv_stage/w2k3-64chk
pscp -p -pw novell \winvmdp\sign\cat\w2k3-64chk\%1* root@%2:/root/driverpack/winpv_stage/w2k3-64chk
pscp -p -pw novell \winvmdp\built\w2k3-64chk\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k3-64chk

REM W2k8 32
pscp -p -pw novell \winvmdp\sign\sys\w2k8-32fre\%1* root@%2:/root/driverpack/winpv_stage/w2k8-32fre
pscp -p -pw novell \winvmdp\sign\cat\w2k8-32fre\%1* root@%2:/root/driverpack/winpv_stage/w2k8-32fre
pscp -p -pw novell \winvmdp\built\w2k8-32fre\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k8-32fre

pscp -p -pw novell \winvmdp\sign\sys\w2k8-32chk\%1* root@%2:/root/driverpack/winpv_stage/w2k8-32chk
pscp -p -pw novell \winvmdp\sign\cat\w2k8-32chk\%1* root@%2:/root/driverpack/winpv_stage/w2k8-32chk
pscp -p -pw novell \winvmdp\built\w2k8-32chk\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k8-32chk

REM W2k8 64
pscp -p -pw novell \winvmdp\sign\sys\w2k8-64fre\%1* root@%2:/root/driverpack/winpv_stage/w2k8-64fre
pscp -p -pw novell \winvmdp\sign\cat\w2k8-64fre\%1* root@%2:/root/driverpack/winpv_stage/w2k8-64fre
pscp -p -pw novell \winvmdp\built\w2k8-64fre\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k8-64fre

pscp -p -pw novell \winvmdp\sign\sys\w2k8-64chk\%1* root@%2:/root/driverpack/winpv_stage/w2k8-64chk
pscp -p -pw novell \winvmdp\sign\cat\w2k8-64chk\%1* root@%2:/root/driverpack/winpv_stage/w2k8-64chk
pscp -p -pw novell \winvmdp\built\w2k8-64chk\%1.pdb root@%2:/root/driverpack/winpv_stage/w2k8-64chk

REM INF
pscp -p -pw novell \winvmdp\built\inf\w2k\%1.inf root@%2:/root/driverpack/winpv_stage/inf/w2k
pscp -p -pw novell \winvmdp\built\inf\%1.inf root@%2:/root/driverpack/winpv_stage/inf/wxp
pscp -p -pw novell \winvmdp\built\inf\%1.inf root@%2:/root/driverpack/winpv_stage/inf/w2k3
pscp -p -pw novell \winvmdp\built\inf\%1.inf root@%2:/root/driverpack/winpv_stage/inf/w2k8
goto end

:vmdp
pscp -p -pw novell \winvmdp\built\w2k-32fre\%1.exe root@%2:/root/driverpack/winpv_stage/w2k-32fre
pscp -p -pw novell \winvmdp\built\wxp-32fre\%1.exe root@%2:/root/driverpack/winpv_stage/wxp-32fre
pscp -p -pw novell \winvmdp\built\w2k3-32fre\%1.exe root@%2:/root/driverpack/winpv_stage/w2k3-32fre
pscp -p -pw novell \winvmdp\built\w2k3-64fre\%1.exe root@%2:/root/driverpack/winpv_stage/w2k3-64fre
pscp -p -pw novell \winvmdp\built\w2k8-32fre\%1.exe root@%2:/root/driverpack/winpv_stage/w2k8-32fre
pscp -p -pw novell \winvmdp\built\w2k8-64fre\%1.exe root@%2:/root/driverpack/winpv_stage/w2k8-64fre

pscp -p -pw novell \winvmdp\built\w2k-32chk\%1.exe root@%2:/root/driverpack/winpv_stage/w2k-32chk
pscp -p -pw novell \winvmdp\built\wxp-32chk\%1.exe root@%2:/root/driverpack/winpv_stage/wxp-32chk
pscp -p -pw novell \winvmdp\built\w2k3-32chk\%1.exe root@%2:/root/driverpack/winpv_stage/w2k3-32chk
pscp -p -pw novell \winvmdp\built\w2k3-64chk\%1.exe root@%2:/root/driverpack/winpv_stage/w2k3-64chk
pscp -p -pw novell \winvmdp\built\w2k8-32chk\%1.exe root@%2:/root/driverpack/winpv_stage/w2k8-32chk
pscp -p -pw novell \winvmdp\built\w2k8-64chk\%1.exe root@%2:/root/driverpack/winpv_stage/w2k8-64chk
goto end

:help
echo.
echo cp2linux.bat copies specified platform files to linux
echo.
echo "syntax: cp2linux <package> <ip_address>"
echo "<package> can be: xenbus, xenblk, xennet, setup, uninstall, pvctrl, xensvc"
echo.

echo example: cp2linux xenbus 151.155.144.121
echo.

:end
