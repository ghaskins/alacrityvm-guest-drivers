#!/bin/sh

if test "$1" = "" || test "$2" = ""
then
	echo "mkver_isos.sh <version> <platform> [<platform ...]"
	exit 1
fi

#OPTIONS="-allow-lowercase -l -relaxed-filenames -r"
ISO_OPTIONS="-r -J -l -d -allow-multidot -allow-leading-dots -no-bak"

cd /root/driverpack

if test -f winpv_isos"$1"
then
	rm -r winpv_isos"$1"
fi

mkdir winpv_isos"$1" 

for i
do
	if test "$i" != "$1"
	then
		if test "$i" = "w2k8"
		then
			./cpwinpv.sh w2k8 fre 32 winpv_tmp $1
			mkisofs $ISO_OPTIONS -V "VMDP WINVLH x86" -o winpv_isos$1/xen-drivers-WinVistaLH-32bit.iso winpv_tmp/

			./cpwinpv.sh w2k8 fre 64 winpv_tmp $1
			mkisofs $ISO_OPTIONS -V "VMDP WINVLH x64" -o winpv_isos$1/xen-drivers-WinVistaLH-64bit.iso winpv_tmp/
		elif test "$i" = "w2k3"
		then
			./cpwinpv.sh w2k3 fre 32 winpv_tmp $1
			mkisofs $ISO_OPTIONS -V "VMDP WIN2003 x86" -o winpv_isos$1/xen-drivers-Win2003-32bit.iso winpv_tmp/

			./cpwinpv.sh w2k3 fre 64 winpv_tmp $1
			mkisofs $ISO_OPTIONS -V "VMDP WIN2003 x64" -o winpv_isos$1/xen-drivers-Win2003-64bit.iso winpv_tmp/
		elif test "$i" = "wxp"
		then
			./cpwinpv.sh wxp fre 32 winpv_tmp $1
			mkisofs $ISO_OPTIONS -V "VMDP WINXP x86" -o winpv_isos$1/xen-drivers-WinXP-32bit.iso winpv_tmp/
			./cpwinpv.sh w2k3 fre 64 winpv_tmp $1
			mkisofs $ISO_OPTIONS -V "VMDP WINXP x64" -o winpv_isos$1/xen-drivers-WinXP-64bit.iso winpv_tmp/
		elif test "$i" = "w2k"
		then
			./cpwinpv.sh w2k fre 32 winpv_tmp $1
			mkisofs $ISO_OPTIONS -V "VMDP WIN2000 x86" -o winpv_isos$1/xen-drivers-Win2000-32bit.iso winpv_tmp/
		else
			echo Unsupported platform: $i
		fi
	fi
done

# No need to create chk whql isos.

rm -r winpv_tmp
