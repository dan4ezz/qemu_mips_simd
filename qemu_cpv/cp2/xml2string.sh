#!/bin/bash
#
# Converts text file to C string file.
#
# usage:
#   xml2string <input_text_file> <variable_name>
#

ifile=$1
varname=$2

echo "char $varname[] = \"\\"
#cat $ifile | sed -n 'l 0' | sed 's/\$/\\n\\/' | sed 's/\"/\\\"/g'
 cat $ifile |                sed 's/$/\\n\\/' | sed 's/\"/\\\"/g'
echo "\";"

