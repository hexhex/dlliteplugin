# $1: instance
# $2: timeout

# default parameters
export PATH=$1
export LD_LIBRARY_PATH=$2
instance=$3
to=$4
owlfile="${instance%%.hex}.owl"
#confstr="-n=1;--supportsets --ontology=$owlfile -n=1;--supportsets --repair=$owlfile -n=1"
confstr="-n=1;--supportsets --repair=$owlfile -n=1"

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
echo -ne "$instance 1"

# for all configurations
i=0
for c in "${confs[@]}"
do
	echo -ne -e " "
	output=$(timeout $to time -o $instance.$i.time.dat -f %e dlvhex2 $c --heuristics=monolithic --liberalsafety --plugindir=../../../../src/ --silent --flpcheck=none $instance > /dev/null)
	ret=$?
	if [[ $ret == 0 ]]; then
	        output="$(cat $instance.$i.time.dat) 0"
	else
		#output=$(cat $instance.$i.time.dat)
		output="--- 1"
	fi
	echo -ne "$output"

	rm $instance.$i.time.dat

	let i=i+1
done
echo -e -ne "\n"
