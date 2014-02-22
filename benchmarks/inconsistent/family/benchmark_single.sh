# $1: instance
# $2: timeout

# default parameters
export PATH=$1
export LD_LIBRARY_PATH=$2
instance=$3
to=$4
owlfile="${instance%%.hex}.owl"
confstr=";--supportsets;--supportsets --repair=$owlfile -n=1;--supportsets --repair=$owlfile --silent"
confstr2=$(cat conf)
if [ $? == 0 ]; then
        confstr=$confstr2
fi

# split configurations
IFS=';' read -ra confs <<< "$confstr"
header="#size"
i=0
for c in "${confs[@]}"
do
	header="$header   \"$c\""
	let i=i+1
done
echo $header

# do benchmark
echo -ne "$instance 1"	# add 1 in order to compute the total number of instances with a certain size

# for all configurations
i=0
for c in "${confs[@]}"
do
	echo -ne -e " "
	output=$(timeout $to time -o $instance.$i.time.dat -f %e dlvhex2 $c --heuristics=monolithic --liberalsafety --plugindir=../../../../src/ --flpcheck=none $instance >$instance.numas)
	linecount=$(cat $instance.numas|wc -l)
	ret=$?
	if [[ $ret == 0 ]]; then
	        output="$(cat $instance.$i.time.dat) 0"	# no timeout --> add 0
	else
		output="--- 1"	# timeout --> add 1
	fi
	echo -ne "$output"
	echo -ne "$linecount"	# Two Options:
				# 1. The good solution: call the aggregation script as follows:
				#    cat *.out | ./aggregate 300 0 0 "3,6,9" "2,4,5,7,8,9" "" ""
				# Columns 3,6,9 will be averaged (contain the runtime for the 3 configurations)
				# Columns 2,4,5,6,7,8,9 are added (2: number of instances, 4,6,8: timeouts for the 3 configurations, 5,7,9: number of answersets for the 3 configurations)
				# (Last two parameters say that we don't compute minimum or maximum of any columns)
				# 2. Quick and dirty (if something does not work and time is pressing):
				#    Add 0 as number of "timeouts" since the aggregation script will sum up every second column, i.e.,
				#	echo -ne "$linecount 0"
				#    (needs to be removed later from the final result)

	rm $instance.$i.time.dat

	let i=i+1
done
echo -e -ne "\n"
