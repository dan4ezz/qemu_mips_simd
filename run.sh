#!/bin/bash
#define dir with algorithms
paths=(
	"CAVLC"
	"CABAC"
	# "ConvolutionMatrix"
	# "MotionCompensation" #не работает
	"MotionEstimation"
	"FDCT-IDCT"
	# "Quantization"
	)

#current directory
cur_directory=$(pwd)

# file containing keys for compiler
param_file=$cur_directory/profiling/parameters.txt

#running algorithm function
# $1 - set of keys for compiler
# $2 - source file
run_alg() {
	if [[ "$2" == *.cpp ]]
	then
		mips-img-linux-gnu-g++ $1 $2 && echo 'done c++'
	else
		mips-img-linux-gnu-gcc-6.3.0 $1 $2 && echo 'done c'
	fi
	# /opt/imgtec/Toolchains/mips-img-linux-gnu/2017.10-05/bin/mips-img-linux-gnu-gcc-6.3.0 $1 $2 && echo 'done'
	$cur_directory/qemu/mips64el-linux-user/qemu-mips64el -cpu I6400 a.out >> /dev/null
}
# counter of keys
counter=25

# main loop: reading file containing keys for compiler
# keys are being written to $keys 
while IFS='' read -r keys || [[ -n "$keys" ]]; do
	counter=$[$counter +1 ]
	# create dir (if not exist) for current set of keys
	mkdir -p $cur_directory/profiling/"$counter"
	# create file defining current set of keys
	echo "$keys">$cur_directory/profiling/"$counter"/key.txt
	for i_path in ${!paths[*]}
	do
		echo "${paths[$i_path]}"
		mkdir -p $cur_directory/profiling/"$counter"/"${paths[$i_path]}"_metrics
		for file in $(ls "${paths[$i_path]}" | grep -E "\.[c,p]{1,3}$")
		do
			echo $file
			cd "${paths[$i_path]}"
			run_alg "$keys" $file
			mv log_msa "${paths[$i_path]}"_log_msa
			mv "${paths[$i_path]}"_log_msa $cur_directory/profiling/"$counter"
			rm -f a.out
			cp main_cnt $cur_directory/profiling/"$counter"/"${paths[$i_path]}"_metrics/
			cd ..
		done
		python3 $cur_directory/Python_parser/parser_v2.py $cur_directory/profiling/"$counter"/"${paths[$i_path]}"_log_msa $cur_directory/profiling/"$counter" "${paths[$i_path]}"_metrics
	done
done < "$param_file"
