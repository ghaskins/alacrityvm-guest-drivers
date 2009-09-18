#!/bin/sh

if test "$1" = ""
then
	echo "cpver_isos.sh <versoin>"
	exit 1
fi

cd /root/driverpack

scp -r winpv_isos$1 xen100:/share/pvdrivers/driverISOs/
ssh root@xen100 "cd /share/pvdrivers/driverISOs/ ; unlink current-$1 ; mv winpv_isos$1 vmdp$1 ; ln -s vmdp$1 current-$1"

