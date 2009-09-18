#!/bin/sh
#OPTIONS="-allow-lowercase -l -relaxed-filenames -r"
ISO_OPTIONS="-r -J -l -d -allow-multidot -allow-leading-dots -no-bak"

cd /root/driverpack
if test -f winpv_isos
then
	rm -r winpv_isos
fi
mkdir winpv_isos

./cpwinpv.sh w2k fre 32 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WIN2000 x86" -o winpv_isos/xen-drivers-Win2000-32bit.iso winpv_tmp/

./cpwinpv.sh wxp fre 32 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WINXP x86" -o winpv_isos/xen-drivers-WinXP-32bit.iso winpv_tmp/

./cpwinpv.sh w2k3 fre 32 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WIN2003 x86" -o winpv_isos/xen-drivers-Win2003-32bit.iso winpv_tmp/

./cpwinpv.sh w2k3 fre 64 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WIN2003 x64" -o winpv_isos/xen-drivers-Win2003-64bit.iso winpv_tmp/
mkisofs $ISO_OPTIONS -V "VMDP WINXP x64" -o winpv_isos/xen-drivers-WinXP-64bit.iso winpv_tmp/

./cpwinpv.sh w2k8 fre 32 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WINVLH x86" -o winpv_isos/xen-drivers-WinVistaLH-32bit.iso winpv_tmp/

./cpwinpv.sh w2k8 fre 64 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WINVLH x64" -o winpv_isos/xen-drivers-WinVistaLH-64bit.iso winpv_tmp/

# Now do the check builds

./cpwinpv.sh w2k chk 32 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WIN2000 x86" -o winpv_isos/xen-drivers-Win2000chk-32bit.iso winpv_tmp/

./cpwinpv.sh wxp chk 32 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WINXP x86" -o winpv_isos/xen-drivers-WinXPchk-32bit.iso winpv_tmp/

./cpwinpv.sh w2k3 chk 32 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WIN2003 x86" -o winpv_isos/xen-drivers-Win2003chk-32bit.iso winpv_tmp/

./cpwinpv.sh w2k3 chk 64 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WIN2003 x64" -o winpv_isos/xen-drivers-Win2003chk-64bit.iso winpv_tmp/
mkisofs $ISO_OPTIONS -V "VMDP WINXP x64" -o winpv_isos/xen-drivers-WinXPchk-64bit.iso winpv_tmp/

./cpwinpv.sh w2k8 chk 32 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WINVLH x86" -o winpv_isos/xen-drivers-WinVistaLHchk-32bit.iso winpv_tmp/

./cpwinpv.sh w2k8 chk 64 winpv_tmp
mkisofs $ISO_OPTIONS -V "VMDP WINVLH x64" -o winpv_isos/xen-drivers-WinVistaLHchk-64bit.iso winpv_tmp/

rm -r winpv_tmp
