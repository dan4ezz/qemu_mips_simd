#!/bin/sh

PORT1=$1
PORT2=$2

FIFO1=FIFO$PORT1-$PORT2
FIFO2=FIFO$PORT2-$PORT1

echo "Connector: $PORT1 <-> $PORT2"
rm -f $FIFO1 $FIFO2
mkfifo $FIFO1
mkfifo $FIFO2
cat < $FIFO2 > $FIFO1 &
nc6 --disable-nagle localhost $PORT1 < $FIFO1 | nc6 --disable-nagle localhost $PORT2 > $FIFO2
