#!/bin/sh

cd /root/driverpack

./cpwinpv.sh w2k fre 32 w2k-32
./cpwinpv.sh wxp fre 32 xp-32
./cpwinpv.sh w2k3 fre 32 w2k3-32
./cpwinpv.sh w2k3 fre 64 w2k3-64
./cpwinpv.sh w2k8 fre 32 wvlh-32
./cpwinpv.sh w2k8 fre 64 wvlh-64

rm vmdp/vmdp-xen-windows.tar.bz2
rm vmdp/vmdp-doc.tar.bz2
tar cjf vmdp/vmdp-xen-windows.tar.bz2 w2k* xp-32 wvlh*
#tar cjf vmdp/vmdp-xen-linux.tar.bz2 rhel*
cd doc
mkdir vmdp
#mv * vmdp
cd vmdp
cp -p ../../winpv_base/doc/* .
unzip -o susedriversforlinux_html.zip
unzip -o susedriversforwindows_html.zip
rm *.zip
cd ..
tar cjf ../vmdp/vmdp-doc.tar.bz2 vmdp
rm -r vmdp
cd ..

# Now that we have the tar fils, remove the directories needed to create them.
rm -r w2k-32
rm -r xp-32
rm -r w2k3-32
rm -r w2k3-64
rm -r wvlh-32
rm -r wvlh-64

