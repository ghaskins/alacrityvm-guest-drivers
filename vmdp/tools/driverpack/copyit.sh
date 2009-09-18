#!/bin/sh

DIRNAME=$(date +%F-%H-%M)
mkdir $DIRNAME
cp xen-drivers*.iso $DIRNAME
scp -r $DIRNAME xen100:/share/pvdrivers/driverISOs/
ssh root@xen100 "cd /share/pvdrivers/driverISOs/ ; unlink current ; ln -s $DIRNAME current"
rm -r $DIRNAME
