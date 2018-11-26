#!/bin/sh
dirpath=$(dirname $0)
. $dirpath/../rio_tests.sh

switch 2020
switch 2030
switch 2040
connector 2020 2031
connector 2030 2041
connector 2040 2021
endpoint 2022
endpoint 2032
endpoint 2042

