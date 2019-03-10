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
param_file=$cur_directory/profiling/metrics/parameters.txt

flag=0
true=1
#running algorithm function
# $1 - set of keys for compiler
# $2 - source file
run_alg() {
	flag=0
	if [[ "$2" == *.cpp ]]
	then
		mips-img-linux-gnu-g++ $1 $2 && flag=1 #echo 'done c++'
	else
		mips-img-linux-gnu-gcc-6.3.0 $1 $2 && flag=1 #echo 'done c'
	fi
	if [ "$flag" -eq "$true" ]
	then
		$cur_directory/qemu/mips64el-linux-user/qemu-mips64el -cpu I6400 a.out >> /dev/null
	fi
}
# counter of keys
counter=0

# main loop: reading file containing keys for compiler
# keys are being written to $keys 
while IFS='' read -r keys || [[ -n "$keys" ]]; do
	counter=$[$counter +1 ]
	# create dir (if not exist) for current set of keys
	mkdir -p $cur_directory/profiling/metrics/"$counter"
	# create file defining current set of keys
	echo "$keys">$cur_directory/profiling/metrics/"$counter"/key.txt
	for i_path in ${!paths[*]}
	do
		mkdir -p $cur_directory/profiling/metrics/"$counter"/"${paths[$i_path]}"_metrics
		for file_without_algo in $(ls "${paths[$i_path]}"/run1 | grep -E "_without_algo\.[c,p]{1,3}$")
		do
			cd "${paths[$i_path]}"/run1
			run_alg "$keys" $file_without_algo
			rm -f a.out
			python3 $cur_directory/Python_parser/parser_v2.py $cur_directory/"${paths[$i_path]}"/run1/log_msa $cur_directory/profiling/metrics/"$counter" "${paths[$i_path]}"_metrics main && echo main_cnt create
			rm -f log_msa
			cd ..
			cd ..
		done
		for file in $(ls "${paths[$i_path]}"/run1 | grep -E "[n,y]{1}\.[c,p]{1,3}$")
		do
			# echo $file
			cd "${paths[$i_path]}"/run1
			run_alg "$keys" $file
			mv log_msa "${paths[$i_path]}"_log_msa
			mv "${paths[$i_path]}"_log_msa $cur_directory/profiling/metrics/"$counter"
			rm -f a.out
			cd ..
			cd ..
		done
		if [ "$flag" -eq "$true" ]
		then
			python3 $cur_directory/Python_parser/parser_v2.py $cur_directory/profiling/metrics/"$counter"/"${paths[$i_path]}"_log_msa $cur_directory/profiling/metrics/"$counter" "${paths[$i_path]}"_metrics
		fi
	done
done < "$param_file"
