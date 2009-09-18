if not exist %1\dist\sign goto help

cd %1\dist\sign\sys\w2k-32fre
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k-32fre\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k-32fre
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\w2k-32chk
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k-32chk\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k-32chk
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\wxp-32fre
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\wxp-32fre\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\wxp-32fre
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\wxp-32chk
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\wxp-32chk\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\wxp-32chk
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\w2k3-32fre
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k3-32fre\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k3-32fre
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\w2k3-32chk
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k3-32chk\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k3-32chk
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\w2k3-64fre
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k3-64fre\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k3-64fre
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\w2k3-64chk
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k3-64chk\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k3-64chk
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\w2k8-32fre
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k8-32fre\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k8-32fre
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\w2k8-32chk
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k8-32chk\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k8-32chk
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\w2k8-64fre
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k8-64fre\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k8-64fre
del *.cdf
del *.inf
del *.cat

cd %1\dist\sign\sys\w2k8-64chk
copy /y %1\dist\built\cdf\*.cdf
copy /y %1\dist\built\w2k8-64chk\*.inf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenbus.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xenblk.cdf
\WinDDK\6001.18002\bin\SelfSign\makecat.exe .\xennet.cdf
copy /y *.cat %1\dist\sign\cat\w2k8-64chk
del *.cdf
del *.inf
del *.cat

if not exist %1\dist\sign\winpvcat.zip goto createzip
copy /y %1\dist\sign\winpvcat.zip %1\dist\prev_sign
del %1\dist\sign\winpvcat.zip
:createzip
"\Program Files\7-Zip\7z.exe" a %1\dist\sign\winpvcat.zip %1\dist\sign\cat
goto end

:help
echo "%1\dist\sign doest not exist."
goto end

:end
