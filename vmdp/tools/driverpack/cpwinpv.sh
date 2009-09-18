#!/bin/sh

if test "$1" = "" || test "$2" = "" || test "$3" = "" || test "$4" = ""
then
	echo "cpwinpv.sh <platform> <release> <arch> <dir> [version]"
	exit 1
fi

cd /root/driverpack

if test -f "$4"
then
	rm -r $4
fi

mkdir $4
cp -p winpv_base/README.txt $4
cp -p winpv_base/base/* $4
cp -p winpv_base/doc/susedriversforwindows.pdf $4
cp -p -r winpv_base/eula $4

if test "$3" = "32"
then
	cp -p winpv_base/x86/* $4

else
	cp -p winpv_base/x64/* $4
fi

cp -p winpv_base/inf/$1/*.inf $4
cp -p winpv_base/$1-$3$2/*sys $4
cp -p winpv_base/$1-$3$2/*cat $4
cp -p winpv_base/$1-$3$2/*exe $4

if test "$2" = "chk"
then
	cp -p winpv_base/$1-$3$2/*pdb $4
fi

# Now copy the specific files
cp -p winpv_stage$5/inf/$1/*inf $4
cp -p winpv_stage$5/$1-$3$2/* $4

if test "$2" = "fre"
then
	rm $4/*pdb
fi

