# $1 starting probability
# $2 ending probability
# $3 step
# $4 number of instances

if [[ $# -lt 4 ]]; then
	echo "Error: Script expects 4 parameters
		1: starting probability
		2: finishing probability
		3: step
		4: number of instances	"
	exit 1;
fi


# create a directory for storing benchmark instances

if [ -d "instances" ]; then
	rm instances/*.*
else
	mkdir -p instances
fi


	for (( concprop=$1; concprop <= $2; concprop+=$3 ))
	do
		for (( inst=0; inst < $4; inst++ ))
		do
			rp=`printf "%03d" ${concprop}`
			in=`printf "%03d" ${inst}`

			# create ontology instances
			./generate.sh 70 ${concprop} > "instances/inst_prop_${rp}_inst_${in}.hex"
			cp ontology.owl instances/inst_prop_${rp}_inst_${in}.owl

			# instantiate the program
			#cat program.hex | sed "s/OWLONTOLOGY/\"inst_prop_${rp}_inst_${in}.owl\"/g" >> "instances/inst_prop_${rp}_inst_${in}.hex"
			cat program.hex | sed "s/OWLONTOLOGY/\"instances\/inst_prop_${rp}_inst_${in}.owl\"/g" >> "instances/inst_prop_${rp}_inst_${in}.hex"	
		done

	done
