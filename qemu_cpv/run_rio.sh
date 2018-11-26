#!/bin/sh

BIOS=vrio/pmon.bin

dirpath=$(dirname $0)
if [ "$1" = "" ]; then
	echo "usage: run_rio.sh <vrio switch tcp port>"
	echo "e.g. run_rio.sh 2021"
	exit 1
fi

$dirpath/mips64-softmmu/qemu-system-mips64 \
		-nographic -m 256 -M duna \
		-bios $BIOS -serial stdio -monitor none \
		-chardev pipe,id=rio,path=$1
