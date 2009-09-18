cd w2k3-32
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.sys xenbus.sys xennet.sys
..\makecat.exe .\xenblk.cdf
..\makecat.exe .\xenbus.cdf
..\makecat.exe .\xennet.cdf
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.cat xenbus.cat xennet.cat
cd ..

cd w2k3-64
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.sys xenbus.sys xennet.sys
..\makecat.exe .\xenblk.cdf
..\makecat.exe .\xenbus.cdf
..\makecat.exe .\xennet.cdf
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.cat xenbus.cat xennet.cat
cd ..

cd wvlh-32
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.sys xenbus.sys xennet.sys
..\makecat.exe .\xenblk.cdf
..\makecat.exe .\xenbus.cdf
..\makecat.exe .\xennet.cdf
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.cat xenbus.cat xennet.cat
cd ..

cd wvlh-64
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.sys xenbus.sys xennet.sys
..\makecat.exe .\xenblk.cdf
..\makecat.exe .\xenbus.cdf
..\makecat.exe .\xennet.cdf
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.cat xenbus.cat xennet.cat
cd ..

cd w2k-32
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.sys xenbus.sys xennet.sys
..\makecat.exe .\xenblk.cdf
..\makecat.exe .\xenbus.cdf
..\makecat.exe .\xennet.cdf
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.cat xenbus.cat xennet.cat
cd ..

cd xp-32
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.sys xenbus.sys xennet.sys
..\makecat.exe .\xenblk.cdf
..\makecat.exe .\xenbus.cdf
..\makecat.exe .\xennet.cdf
c:\CodeSigning\SignTools\Microsoft\vista\signtool.exe sign /v /ac C:\codesigning\SignTools\Microsoft\vista\MSCV-VSClass3.cer /sha1 53D43A551D0ADDE9F762AA36888B37E79FA6865A /v xenblk.cat xenbus.cat xennet.cat
cd ..

echo file(s) need to be timestamped
pause
