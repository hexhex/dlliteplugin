# $1: instance
# $2: timeout

# default parameters
export PATH=$1
export LD_LIBRARY_PATH=$2
instance=$3
to=$4

confstr=";--supportsets;-n=1;--supportsets -n=1"
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
echo -ne "$instance"

# for all configurations
i=0
for c in "${confs[@]}"
do
	echo -ne -e " "
	output=$(timeout $to time -o $instance.$i.time.dat -f %e dlvhex2 $c --heuristics=monolithic --plugindir=../../../src/ $instance --verbose=8 2>$instance.$i.verbose.dat > /dev/null)
	ret=$?
	if [[ $ret == 0 ]]; then
	        output=$(cat $instance.$i.time.dat)
		groundertime=$(cat $instance.$i.verbose.dat | grep -a "HEX grounder time:" | tail -n 1 | grep -P -o '[0-9]+\.[0-9]+s' | sed "s/s//")
	        solvertime=$(cat $instance.$i.verbose.dat | grep -a "HEX solver time:" | tail -n 1 | grep -P -o '[0-9]+\.[0-9]+s' | sed "s/s//")
	else
		output="---"
		groundertime="---"
		solvertime="---"
	fi
	echo -ne "$output $groundertime $solvertime"

	rm $instance.$i.time.dat
#	rm $instance.$i.verbose.dat

	let i=i+1
done
echo -e -ne "\n"
