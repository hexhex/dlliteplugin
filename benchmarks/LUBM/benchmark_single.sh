# $1: instance
# $2: timeout

# default parameters
export PATH=$1
export LD_LIBRARY_PATH=$2
instance=$3
to=$4

owlfile="${instance%%.hex}.owl"
confstr="dlvhex2 --heuristics=monolithic --plugindir=../../../src/ --ontology=$owlfile $instance;dlvhex2 --heuristics=monolithic --plugindir=../../../src/ --ontology=$owlfile $instance --supportsets;../calldrew.sh $instance $owlfile;dlvhex2 --heuristics=monolithic --plugindir=../../../src/ --ontology=$owlfile $instance -n=1;dlvhex2 --heuristics=monolithic --plugindir=../../../src/ --ontology=$owlfile $instance --supportsets -n=1"
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
	output=$(timeout $to time -o $instance.$i.time.dat -f %e $c > /dev/null 2> $instance.$i.verbose.dat)
	ret=$?

	# drew does not notify the caller about errors, therefore we need this hack
	err=$(cat $instance.$i.verbose.dat | grep "Exception" | grep "java" | wc -l)
	if [[ $err -ge 1 ]]; then
		ret=1
	fi

	if [[ $ret == 0 ]]; then
	        output=$(cat $instance.$i.time.dat)
	else
		output="---"
	fi
	echo -ne "$output"

	rm $instance.$i.time.dat
	rm $instance.$i.verbose.dat

	let i=i+1
done
echo -e -ne "\n"
