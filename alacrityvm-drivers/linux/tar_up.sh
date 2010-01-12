#!/bin/bash

set -e


TMP=$(mktemp -d)
if [ $? -ne 0 ]; then
    echo "$0: Can't create temp dir, exiting." >&2
    exit -1
fi

cleanup() {
    rm -rf $TMP
}

handle_terminate() {
    echo "(received signal)

Interrupted, cleaning up." >&2
    cleanup
    exit 255
}

error() {
    cleanup
    exit -1
}

trap handle_terminate 1 2 3 15

EXECDIR=$(dirname $0)
KSRC=$1

HDRS="shm_signal.h ioq.h vbus_driver.h vbus_pci.h venet.h"
SRCS="lib/shm_signal.c lib/ioq.c drivers/vbus/bus-proxy.c drivers/vbus/pci-bridge.c drivers/net/vbus-enet.c"
LOCALS="Makefile.base alacrityvm-drivers.spec COPYING"

mkdir -p $TMP/linux

for i in $HDRS
do
	SRC=$KSRC/include/linux/$i
	DST=$TMP/linux/$i
	echo "Copy from $SRC to $DST" >&2
	cp $SRC $DST
done

for i in $SRCS
do
	SRC=$KSRC/$i
	DST=$TMP/$(basename $i)
	echo "Copy from $SRC to $DST" >&2
	cp $SRC $DST
done

for i in $LOCALS
do
	SRC=$EXECDIR/$i
	DST=$TMP/$(echo $i | sed 's/.base//')
	echo "Copy from $SRC to $DST" >&2
	cp $SRC $DST
done

cd $TMP
tar -c *

cleanup
