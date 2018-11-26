#!/bin/sh
rio_test_dirpath=$(dirname $0)

endpoint () {
	port=$1

	rm -f $port.in
	rm -f $port.out

	mkfifo $port.in
	mkfifo $port.out

        (cat $port.out | (while true; do socat TCP:localhost:$port -; if [ "$?" != "0" ]; then continue; fi; break; done) | cat > $port.in) &

	$rio_test_dirpath/../../run_rio.sh $port
}
switch () {
	port=$1
	portnumb=$2
	$rio_test_dirpath/../vrio_switch.py -P $port
}
connector () {
	port1=$1
	port2=$2
	$rio_test_dirpath/../vrio_connector.sh $port1 $port2
}
e () {
	endpoint $1
}
s () {
	switch $1 $2
}
c () {
	connector $1 $2
}
