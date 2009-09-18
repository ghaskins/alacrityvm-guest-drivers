#!/bin/sh
rm vmdp-xen-windows.tar.bz2
rm vmdp-doc.tar.bz2
tar cjf vmdp-xen-windows.tar.bz2 w2k* xp-32 wvlh*
tar cjf vmdp-xen-linux.tar.bz2 rhel*
cd doc
rm -r vmdp
mkdir vmdp
mv * vmdp
cd vmdp
unzip -o susedriversforlinux_html.zip
unzip -o susedriversforwindows_html.zip
rm *.zip
cd ..
tar cjf ../vmdp-doc.tar.bz2 vmdp
cd ..
