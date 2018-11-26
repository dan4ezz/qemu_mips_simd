#!/bin/sh
dirpath=$(dirname $0)
. $dirpath/../rio_tests.sh

switch 2020
switch 2030
connector 2020 2030
endpoint 2021
endpoint 2031
