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
	output=$(timeout $to time -o $instance.$i.time.dat -f %e dlvhex2 $c --heuristics=monolithic --plugindir=../../../src/ $instance --silent > $instance.$i.cnt.dat)
	ret=$?
	if [[ $ret == 0 ]]; then
	        output=$(cat $instance.$i.time.dat)
		cnt=$(cat $instance.$i.cnt.dat | wc -l)
	else
		output="---"
		cnt="---"
	fi
	echo -ne "$output $cnt"

	rm $instance.$i.time.dat
	rm $instance.$i.cnt.dat

	let i=i+1
done
echo -e -ne "\n"
