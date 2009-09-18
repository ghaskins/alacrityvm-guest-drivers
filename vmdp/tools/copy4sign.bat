@echo off
REM copy4sign copys all files in preparation for signing and packaging.

REM :win2k-32bit-fre
copy /y %1\vmdp\dist\win2k\fre\i386\*.sys %1\dist\sign\sys\w2k-32fre
copy /y %1\vmdp\dist\win2k\fre\i386\*.pdb %1\dist\built\w2k-32fre

REM :win2k-32bit-chk
copy /y %1\vmdp\dist\win2k\chk\i386\*.sys %1\dist\sign\sys\w2k-32chk
copy /y %1\vmdp\dist\win2k\chk\i386\*.pdb %1\dist\built\w2k-32chk

REM:winxp-32bit-fre
copy /y %1\vmdp\dist\winxp\fre\i386\*.sys %1\dist\sign\sys\wxp-32fre
copy /y %1\vmdp\dist\winxp\fre\i386\*.pdb %1\dist\built\wxp-32fre

REM:winxp-32bit-chk
copy /y %1\vmdp\dist\winxp\chk\i386\*.sys %1\dist\sign\sys\wxp-32chk
copy /y %1\vmdp\dist\winxp\chk\i386\*.pdb %1\dist\built\wxp-32chk

REM:win2k3-32bit-fre
copy /y %1\vmdp\dist\winnet\fre\i386\*.sys %1\dist\sign\sys\w2k3-32fre
copy /y %1\vmdp\dist\winnet\fre\i386\*.pdb %1\dist\built\w2k3-32fre

REM:win2k3-32bit-chk
copy /y %1\vmdp\dist\winnet\chk\i386\*.sys %1\dist\sign\sys\w2k3-32chk
copy /y %1\vmdp\dist\winnet\chk\i386\*.pdb %1\dist\built\w2k3-32chk

REM:win2k3-64bit-fre
copy /y %1\vmdp\dist\winnet\fre\amd64\*.sys %1\dist\sign\sys\w2k3-64fre
copy /y %1\vmdp\dist\winnet\fre\amd64\*.pdb %1\dist\built\w2k3-64fre

REM:win2k3-64bit-chk
copy /y %1\vmdp\dist\winnet\chk\amd64\*.sys %1\dist\sign\sys\w2k3-64chk
copy /y %1\vmdp\dist\winnet\chk\amd64\*.pdb %1\dist\built\w2k3-64chk

REM:winvlh-32bit-fre
copy /y %1\vmdp\dist\winlh\fre\i386\*.sys %1\dist\sign\sys\w2k8-32fre
copy /y %1\vmdp\dist\winlh\fre\i386\*.pdb %1\dist\built\w2k8-32fre

REM:winvlh-32bit-chk
copy /y %1\vmdp\dist\winlh\chk\i386\*.sys %1\dist\sign\sys\w2k8-32chk
copy /y %1\vmdp\dist\winlh\chk\i386\*.pdb %1\dist\built\w2k8-32chk

REM:winvlh-64bit-fre
copy /y %1\vmdp\dist\winlh\fre\amd64\*.sys %1\dist\sign\sys\w2k8-64fre
copy /y %1\vmdp\dist\winlh\fre\amd64\*.pdb %1\dist\built\w2k8-64fre

REM:winvlh-64bit-chk
copy /y %1\vmdp\dist\winlh\chk\amd64\*.sys %1\dist\sign\sys\w2k8-64chk
copy /y %1\vmdp\dist\winlh\chk\amd64\*.pdb %1\dist\built\w2k8-64chk

REM:mainsetup
copy /y %1\vmdp\mainsetup\objfre_w2k_x86\i386\setup.exe %1\dist\sign\exe\main-fre
copy /y %1\vmdp\mainsetup\objchk_w2k_x86\i386\setup.exe %1\dist\sign\exe\main-chk

REM :win2k-32bit-fre
copy /y %1\vmdp\setup\objfre_w2k_x86\i386\setup.exe %1\dist\sign\exe\w2k-32fre
copy /y %1\vmdp\uninstall\objfre_w2k_x86\i386\uninstall.exe %1\dist\sign\exe\w2k-32fre
copy /y %1\vmdp\xensvc\objfre_w2k_x86\i386\xensvc.exe %1\dist\sign\exe\w2k-32fre
copy /y %1\vmdp\pvctrl\objfre_w2k_x86\i386\pvctrl.exe %1\dist\sign\exe\w2k-32fre


REM :win2k-32bit-chk
copy /y %1\vmdp\setup\objchk_w2k_x86\i386\setup.exe %1\dist\sign\exe\w2k-32chk
copy /y %1\vmdp\uninstall\objchk_w2k_x86\i386\uninstall.exe %1\dist\sign\exe\w2k-32chk
copy /y %1\vmdp\xensvc\objchk_w2k_x86\i386\xensvc.exe %1\dist\sign\exe\w2k-32chk
copy /y %1\vmdp\pvctrl\objchk_w2k_x86\i386\pvctrl.exe %1\dist\sign\exe\w2k-32chk

REM:winxp-32bit-fre
copy /y %1\vmdp\setup\objfre_wxp_x86\i386\setup.exe %1\dist\sign\exe\wxp-32fre
copy /y %1\vmdp\uninstall\objfre_wxp_x86\i386\uninstall.exe %1\dist\sign\exe\wxp-32fre
copy /y %1\vmdp\xensvc\objfre_wxp_x86\i386\xensvc.exe %1\dist\sign\exe\wxp-32fre
copy /y %1\vmdp\pvctrl\objfre_wxp_x86\i386\pvctrl.exe %1\dist\sign\exe\wxp-32fre

REM:winxp-32bit-chk
copy /y %1\vmdp\setup\objchk_wxp_x86\i386\setup.exe %1\dist\sign\exe\wxp-32chk
copy /y %1\vmdp\uninstall\objchk_wxp_x86\i386\uninstall.exe %1\dist\sign\exe\wxp-32chk
copy /y %1\vmdp\xensvc\objchk_wxp_x86\i386\xensvc.exe %1\dist\sign\exe\wxp-32chk
copy /y %1\vmdp\pvctrl\objchk_wxp_x86\i386\pvctrl.exe %1\dist\sign\exe\wxp-32chk

REM:win2k3-32bit-fre
copy /y %1\vmdp\setup\objfre_wnet_x86\i386\setup.exe %1\dist\sign\exe\w2k3-32fre
copy /y %1\vmdp\uninstall\objfre_wnet_x86\i386\uninstall.exe %1\dist\sign\exe\w2k3-32fre
copy /y %1\vmdp\xensvc\objfre_wnet_x86\i386\xensvc.exe %1\dist\sign\exe\w2k3-32fre
copy /y %1\vmdp\pvctrl\objfre_wnet_x86\i386\pvctrl.exe %1\dist\sign\exe\w2k3-32fre

REM:win2k3-32bit-chk
copy /y %1\vmdp\setup\objchk_wnet_x86\i386\setup.exe %1\dist\sign\exe\w2k3-32chk
copy /y %1\vmdp\uninstall\objchk_wnet_x86\i386\uninstall.exe %1\dist\sign\exe\w2k3-32chk
copy /y %1\vmdp\xensvc\objchk_wnet_x86\i386\xensvc.exe %1\dist\sign\exe\w2k3-32chk
copy /y %1\vmdp\pvctrl\objchk_wnet_x86\i386\pvctrl.exe %1\dist\sign\exe\w2k3-32chk

REM:win2k3-64bit-fre
copy /y %1\vmdp\setup\objfre_wnet_amd64\amd64\setup.exe %1\dist\sign\exe\w2k3-64fre
copy /y %1\vmdp\uninstall\objfre_wnet_amd64\amd64\uninstall.exe %1\dist\sign\exe\w2k3-64fre
copy /y %1\vmdp\xensvc\objfre_wnet_amd64\amd64\xensvc.exe %1\dist\sign\exe\w2k3-64fre
copy /y %1\vmdp\pvctrl\objfre_wnet_amd64\amd64\pvctrl.exe %1\dist\sign\exe\w2k3-64fre

REM:win2k3-64bit-chk
copy /y %1\vmdp\setup\objchk_wnet_amd64\amd64\setup.exe %1\dist\sign\exe\w2k3-64chk
copy /y %1\vmdp\uninstall\objchk_wnet_amd64\amd64\uninstall.exe %1\dist\sign\exe\w2k3-64chk
copy /y %1\vmdp\xensvc\objchk_wnet_amd64\amd64\xensvc.exe %1\dist\sign\exe\w2k3-64chk
copy /y %1\vmdp\pvctrl\objchk_wnet_amd64\amd64\pvctrl.exe %1\dist\sign\exe\w2k3-64chk

REM:winvlh-32bit-fre
copy /y %1\vmdp\setup\objfre_wlh_x86\i386\setup.exe %1\dist\sign\exe\w2k8-32fre
copy /y %1\vmdp\uninstall\objfre_wlh_x86\i386\uninstall.exe %1\dist\sign\exe\w2k8-32fre
copy /y %1\vmdp\xensvc\objfre_wlh_x86\i386\xensvc.exe %1\dist\sign\exe\w2k8-32fre
copy /y %1\vmdp\pvctrl\objfre_wlh_x86\i386\pvctrl.exe %1\dist\sign\exe\w2k8-32fre

REM:winvlh-32bit-chk
copy /y %1\vmdp\setup\objchk_wlh_x86\i386\setup.exe %1\dist\sign\exe\w2k8-32chk
copy /y %1\vmdp\uninstall\objchk_wlh_x86\i386\uninstall.exe %1\dist\sign\exe\w2k8-32chk
copy /y %1\vmdp\xensvc\objchk_wlh_x86\i386\xensvc.exe %1\dist\sign\exe\w2k8-32chk
copy /y %1\vmdp\pvctrl\objchk_wlh_x86\i386\pvctrl.exe %1\dist\sign\exe\w2k8-32chk

REM:winvlh-64bit-fre
copy /y %1\vmdp\setup\objfre_wlh_amd64\amd64\setup.exe %1\dist\sign\exe\w2k8-64fre
copy /y %1\vmdp\uninstall\objfre_wlh_amd64\amd64\uninstall.exe %1\dist\sign\exe\w2k8-64fre
copy /y %1\vmdp\xensvc\objfre_wlh_amd64\amd64\xensvc.exe %1\dist\sign\exe\w2k8-64fre
copy /y %1\vmdp\pvctrl\objfre_wlh_amd64\amd64\pvctrl.exe %1\dist\sign\exe\w2k8-64fre

REM:winvlh-64bit-chk
copy /y %1\vmdp\setup\objchk_wlh_amd64\amd64\setup.exe %1\dist\sign\exe\w2k8-64chk
copy /y %1\vmdp\uninstall\objchk_wlh_amd64\amd64\uninstall.exe %1\dist\sign\exe\w2k8-64chk
copy /y %1\vmdp\xensvc\objchk_wlh_amd64\amd64\xensvc.exe %1\dist\sign\exe\w2k8-64chk
copy /y %1\vmdp\pvctrl\objchk_wlh_amd64\amd64\pvctrl.exe %1\dist\sign\exe\w2k8-64chk
goto end

:help
echo.
echo copy4sign.bat copys driver files files in prepation for
echo signing and packaging.
echo.
echo "syntax: copy4sign [<dir>]"
echo example: copy4sign <dir>
echo example: copy4sign
echo.

:end
