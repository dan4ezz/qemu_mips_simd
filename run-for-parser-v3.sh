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
param_file=$cur_directory/profiling/metrics_v3/param0.txt

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
		# /opt/imgtec/Toolchains/mips-img-linux-gnu/2017.10-05/bin/mips-img-linux-gnu-gcc-6.3.0 $1 $2 && echo 'done'
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
	mkdir -p $cur_directory/profiling/metrics_v3/"$counter"
	# create file defining current set of keys
	echo "$keys">$cur_directory/profiling/metrics_v3/"$counter"/key.txt
	for i_path in ${!paths[*]}
	do
		# echo "${paths[$i_path]}"
		mkdir -p $cur_directory/profiling/metrics_v3/"$counter"/"${paths[$i_path]}"_metrics
		for file_without_algo in $(ls "${paths[$i_path]}"/run1 | grep -E "_without_algo\.[c,p]{1,3}$")
		do
			cd "${paths[$i_path]}"/run1
			run_alg "$keys" $file_without_algo
			rm -f a.out
			mkdir -p log_main
			mv log_msa log_main
			cd ..
			cd ..
		done
		for file in $(ls "${paths[$i_path]}"/run1 | grep -E "[n,y]{1}\.[c,p]{1,3}$")
		do
			cd "${paths[$i_path]}"/run1
			run_alg "$keys" $file
			rm -f a.out
			if [ "$flag" -eq "$true" ]
			then
				python3 $cur_directory/Python_parser/3rd-version/parser_v3.py . log_main $cur_directory/profiling/metrics_v3/"$counter"/"${paths[$i_path]}"_metrics
			fi
			rm -f log_msa
			rm -f new_log
			rm -f log_main/log_msa
			cd ..
			cd ..
		done
	done
done < "$param_file"
