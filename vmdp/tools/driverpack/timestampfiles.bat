cd w2k-32
REM c:\CodeSigning\SignTools\Microsoft\vista\signtool timestamp /t http://timestamp.verisign.com/scripts/timstamp.dll xenblk.sys xenbus.sys xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.cat
cd ..

cd w2k3-32
REM c:\CodeSigning\SignTools\Microsoft\vista\signtool timestamp /t http://timestamp.verisign.com/scripts/timstamp.dll xenblk.sys xenbus.sys xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.cat
cd ..


cd w2k3-64
REM c:\CodeSigning\SignTools\Microsoft\vista\signtool timestamp /t http://timestamp.verisign.com/scripts/timstamp.dll xenblk.sys xenbus.sys xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.cat
cd ..

cd wvlh-32
REM c:\CodeSigning\SignTools\Microsoft\vista\signtool timestamp /t http://timestamp.verisign.com/scripts/timstamp.dll xenblk.sys xenbus.sys xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.cat
cd ..

cd wvlh-64
REM c:\CodeSigning\SignTools\Microsoft\vista\signtool timestamp /t http://timestamp.verisign.com/scripts/timstamp.dll xenblk.sys xenbus.sys xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.cat
cd ..

cd xp-32
REM c:\CodeSigning\SignTools\Microsoft\vista\signtool timestamp /t http://timestamp.verisign.com/scripts/timstamp.dll xenblk.sys xenbus.sys xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.sys
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenblk.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xenbus.cat
..\signcode.exe -t http://timestamp.verisign.com/scripts/timstamp.dll -x xennet.cat
cd ..

echo files have been timestamped
pause
