#!/bin/sh

if test "$1" = ""
then
	echo "cpisos.sh <versoin>"
	exit 1
fi

cd /root/driverpack

#DIRNAME=$(date +%F-%H-%M)
DIRNAME=vmdp$1
#mv winpv_stage winpv_stage$1
scp -r winpv_isos xen100:/share/pvdrivers/driverISOs/
ssh root@xen100 "cd /share/pvdrivers/driverISOs/ ; unlink current ; mv winpv_isos $DIRNAME ; ln -s $DIRNAME current"
#rm -r winpv_isos$1
#mv winpv_isos winpv_isos$1
#./mkwinpvdirs.sh

