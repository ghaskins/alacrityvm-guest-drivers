::
:: Simple batch script to install both vbus and venet drivers.
:: Assumes that the current system is win7 32-bit, and the test cert in 
:: installed.
::
:: We just copy the needed files to a tmp dir and use devcon to install.
::
:: Note that we always re-create the catalog and sign since we have no doubt modified 
:: sources files.  
::
:: To uninstall, use device manager.  
::
:: Peter W. Morreale    12/01/09

C:

:: Delete old setupapi log.
del \windows\inf\setupapi.dev.log

:: First install vbus, creating the catalog as we go.
mkdir c:\tmp
del /q C:\tmp\*
copy C:\WinDDK\7600.16385.0\redist\wdf\%_BUILDARCH%\WdfCoInstaller01009.dll  C:\tmp
copy Z:\vbus\objchk_win7_x86\i386\vbus.sys C:\tmp
copy Z:\vbus\objchk_win7_x86\i386\vbus.inf C:\tmp
cd \tmp
inf2cat /driver:C:\tmp /os:7_X86
signtool sign /v /s root /n vbus /t http://verisign.timestamp.com/scripts/timestamp.dll vbus.cat
devcon update  vbus.inf PCI\VEN_11DA^&DEV_2000

:: Now install venet, again creating the catalog.
del /q C:\tmp\*
copy C:\WinDDK\7600.16385.0\redist\wdf\%_BUILDARCH%\WdfCoInstaller01009.dll  C:\tmp
copy Z:\venet\objchk_win7_x86\i386\venet.sys \tmp
copy Z:\venet\objchk_win7_x86\i386\netnovl.inf \tmp
cd \tmp
inf2cat /driver:C:\tmp /os:7_X86
signtool sign /v /s root /n vbus /t http://verisign.timestamp.com/scripts/timestamp.dll venet.cat
devcon update  netnovl.inf VBUS\test_dev_10

:: copy setup log...

copy \windows\inf\setupapi.dev.log z:setup.log

Z:
