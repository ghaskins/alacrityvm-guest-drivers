:: make and install a test cert.

z:
cd \cert
del /q *

makecert -r  -pe -ss root -n "CN=vbus" mycert.cer
certmgr /add mycert.cer /s /r localMachine root
