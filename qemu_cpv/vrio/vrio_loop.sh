#!/bin/sh

PORT1=$1
PORT2=$2

FIFO1=01
FIFO2=10

rm -f $FIFO1 $FIFO2
mkfifo $FIFO1
mkfifo $FIFO2
cat < $FIFO2 > $FIFO1 &
nc6 --disable-nagle -l localhost -p $PORT1 < $FIFO1 | nc6 --disable-nagle -l localhost -p $PORT2 > $FIFO2
